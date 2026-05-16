#include "FeederScheduleCodec.h"

#include <Esp32At24cRecordStore.h>

#include <cstring>

namespace {

static constexpr uint8_t kPlanFlagEnabled = 0x01;
static constexpr uint8_t kPlanFlagTimeConfigured = 0x02;
static constexpr uint8_t kPlanFlagSkipToday = 0x04;
static constexpr uint8_t kPlanFlagAttemptedToday = 0x08;
static constexpr uint8_t kPlanFlagExecutedToday = 0x10;
static constexpr uint8_t kPlanFlagMissedToday = 0x20;

uint16_t readU16(const uint8_t* bytes) {
  return static_cast<uint16_t>(bytes[0]) | (static_cast<uint16_t>(bytes[1]) << 8);
}

uint32_t readU32(const uint8_t* bytes) {
  return static_cast<uint32_t>(bytes[0]) | (static_cast<uint32_t>(bytes[1]) << 8) |
         (static_cast<uint32_t>(bytes[2]) << 16) | (static_cast<uint32_t>(bytes[3]) << 24);
}

int32_t readI32(const uint8_t* bytes) {
  return static_cast<int32_t>(readU32(bytes));
}

void writeU16(uint8_t* bytes, uint16_t value) {
  bytes[0] = static_cast<uint8_t>(value & 0xFF);
  bytes[1] = static_cast<uint8_t>((value >> 8) & 0xFF);
}

void writeU32(uint8_t* bytes, uint32_t value) {
  bytes[0] = static_cast<uint8_t>(value & 0xFF);
  bytes[1] = static_cast<uint8_t>((value >> 8) & 0xFF);
  bytes[2] = static_cast<uint8_t>((value >> 16) & 0xFF);
  bytes[3] = static_cast<uint8_t>((value >> 24) & 0xFF);
}

void writeI32(uint8_t* bytes, int32_t value) {
  writeU32(bytes, static_cast<uint32_t>(value));
}

uint8_t flagsForPlan(const FeederPlanState& plan) {
  uint8_t flags = 0;
  if (plan.config.enabled) {
    flags |= kPlanFlagEnabled;
  }
  if (plan.config.timeConfigured) {
    flags |= kPlanFlagTimeConfigured;
  }
  if (plan.skipToday) {
    flags |= kPlanFlagSkipToday;
  }
  if (plan.scheduleAttemptedToday) {
    flags |= kPlanFlagAttemptedToday;
  }
  if (plan.todayExecuted) {
    flags |= kPlanFlagExecutedToday;
  }
  if (plan.scheduleMissedToday) {
    flags |= kPlanFlagMissedToday;
  }
  return flags;
}

void applyPlanFlags(uint8_t flags, FeederPlanState& plan) {
  plan.config.enabled = (flags & kPlanFlagEnabled) != 0;
  plan.config.timeConfigured = (flags & kPlanFlagTimeConfigured) != 0;
  plan.skipToday = (flags & kPlanFlagSkipToday) != 0;
  plan.scheduleAttemptedToday = (flags & kPlanFlagAttemptedToday) != 0;
  plan.todayExecuted = (flags & kPlanFlagExecutedToday) != 0;
  plan.scheduleMissedToday = (flags & kPlanFlagMissedToday) != 0;
}

bool validSnapshot(const FeederScheduleSnapshot& snapshot) {
  if (snapshot.planCount > kFeederMaxPlans) {
    return false;
  }
  for (uint8_t i = 0; i < snapshot.planCount; ++i) {
    const FeederPlanConfig& config = snapshot.plans[i].config;
    if (config.timeMinutes >= 24 * 60 && config.timeConfigured) {
      return false;
    }
    if ((config.channelMask & static_cast<uint8_t>(~((1U << kFeederMaxChannels) - 1U))) != 0) {
      return false;
    }
  }
  return true;
}

}  // namespace

FeederScheduleCodecResult encodeFeederScheduleSnapshot(const FeederScheduleSnapshot& snapshot,
                                                       uint8_t* out,
                                                       std::size_t capacity,
                                                       std::size_t& encodedLength) {
  encodedLength = 0;
  if (out == nullptr || !validSnapshot(snapshot)) {
    return FeederScheduleCodecResult::InvalidArgument;
  }
  if (capacity < kFeederScheduleEncodedBytes) {
    return FeederScheduleCodecResult::BufferTooSmall;
  }

  std::memset(out, 0, kFeederScheduleEncodedBytes);
  writeU32(out + 0, kFeederScheduleMagic);
  writeU16(out + 4, kFeederScheduleSchemaVersion);
  out[6] = snapshot.planCount;
  out[7] = kFeederMaxPlans;
  writeU32(out + 8, snapshot.serviceDate);

  for (uint8_t i = 0; i < kFeederMaxPlans; ++i) {
    const FeederPlanState& plan = snapshot.plans[i];
    uint8_t* bytes = out + kFeederScheduleHeaderBytes + (i * kFeederSchedulePlanBytes);
    bytes[0] = plan.config.planId;
    bytes[1] = flagsForPlan(plan);
    writeU16(bytes + 2, plan.config.timeMinutes);
    bytes[4] = plan.config.channelMask;
    bytes[5] = 0;
    for (uint8_t channel = 0; channel < kFeederMaxChannels; ++channel) {
      const FeederChannelTarget& target = plan.config.targets[channel];
      uint8_t* targetBytes = bytes + 6 + (channel * 10);
      targetBytes[0] = static_cast<uint8_t>(target.mode);
      targetBytes[1] = 0;
      writeI32(targetBytes + 2, target.targetGramsX100);
      writeI32(targetBytes + 6, target.targetRevolutionsX100);
    }
  }

  const uint32_t crc = Esp32At24cRecordStore::crc32IsoHdlc(
      out + kFeederScheduleHeaderBytes, kFeederScheduleEncodedBytes - kFeederScheduleHeaderBytes);
  writeU32(out + 12, crc);
  encodedLength = kFeederScheduleEncodedBytes;
  return FeederScheduleCodecResult::Ok;
}

FeederScheduleCodecResult verifyFeederScheduleSnapshot(const uint8_t* data, std::size_t length) {
  if (data == nullptr) {
    return FeederScheduleCodecResult::InvalidArgument;
  }
  if (length < kFeederScheduleEncodedBytes) {
    return FeederScheduleCodecResult::BufferTooSmall;
  }
  if (readU32(data + 0) != kFeederScheduleMagic ||
      readU16(data + 4) != kFeederScheduleSchemaVersion || data[7] != kFeederMaxPlans) {
    return FeederScheduleCodecResult::UnsupportedVersion;
  }
  if (data[6] > kFeederMaxPlans) {
    return FeederScheduleCodecResult::InvalidArgument;
  }
  const uint32_t expectedCrc = readU32(data + 12);
  const uint32_t actualCrc = Esp32At24cRecordStore::crc32IsoHdlc(
      data + kFeederScheduleHeaderBytes, kFeederScheduleEncodedBytes - kFeederScheduleHeaderBytes);
  return expectedCrc == actualCrc ? FeederScheduleCodecResult::Ok
                                  : FeederScheduleCodecResult::CrcMismatch;
}

FeederScheduleCodecResult decodeFeederScheduleSnapshot(const uint8_t* data,
                                                       std::size_t length,
                                                       FeederScheduleSnapshot& out) {
  const FeederScheduleCodecResult verifyResult = verifyFeederScheduleSnapshot(data, length);
  if (verifyResult != FeederScheduleCodecResult::Ok) {
    return verifyResult;
  }

  FeederScheduleSnapshot decoded;
  decoded.planCount = data[6];
  decoded.serviceDate = readU32(data + 8);
  for (uint8_t i = 0; i < decoded.planCount; ++i) {
    const uint8_t* bytes = data + kFeederScheduleHeaderBytes + (i * kFeederSchedulePlanBytes);
    FeederPlanState& plan = decoded.plans[i];
    plan.config.planId = bytes[0];
    applyPlanFlags(bytes[1], plan);
    plan.config.timeMinutes = readU16(bytes + 2);
    plan.config.channelMask = bytes[4];
    for (uint8_t channel = 0; channel < kFeederMaxChannels; ++channel) {
      const uint8_t* targetBytes = bytes + 6 + (channel * 10);
      FeederChannelTarget& target = plan.config.targets[channel];
      target.mode = static_cast<FeederTargetMode>(targetBytes[0]);
      target.targetGramsX100 = readI32(targetBytes + 2);
      target.targetRevolutionsX100 = readI32(targetBytes + 6);
    }
  }
  if (!validSnapshot(decoded)) {
    return FeederScheduleCodecResult::InvalidArgument;
  }

  out = decoded;
  return FeederScheduleCodecResult::Ok;
}
