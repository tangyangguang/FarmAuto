#include "FeederBucketCodec.h"

#include <Esp32At24cRecordStore.h>

#include <cstring>

namespace {

static constexpr uint8_t kChannelFlagEnabled = 0x01;
static constexpr uint8_t kChannelFlagUnderflow = 0x02;

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

uint8_t flagsForChannel(const FeederBucketState& channel) {
  uint8_t flags = 0;
  if (channel.baseInfo.enabled) {
    flags |= kChannelFlagEnabled;
  }
  if (channel.underflow) {
    flags |= kChannelFlagUnderflow;
  }
  return flags;
}

bool isEmptyDisabledChannel(const FeederBucketState& channel) {
  return !channel.baseInfo.enabled && channel.baseInfo.outputPulsesPerRev == 0 &&
         channel.baseInfo.gramsPerRevX100 == 0 && channel.baseInfo.capacityGramsX100 == 0 &&
         channel.remainGramsX100 == 0 && channel.lastRefillUnixTime == 0 && !channel.underflow;
}

bool validSnapshot(const FeederBucketSnapshot& snapshot) {
  for (uint8_t i = 0; i < kFeederMaxChannels; ++i) {
    const FeederBucketState& channel = snapshot.channels[i];
    if (isEmptyDisabledChannel(channel)) {
      continue;
    }
    if (channel.baseInfo.outputPulsesPerRev <= 0 || channel.baseInfo.gramsPerRevX100 < 0 ||
        channel.baseInfo.capacityGramsX100 <= 0 || channel.remainGramsX100 < 0 ||
        channel.remainGramsX100 > channel.baseInfo.capacityGramsX100) {
      return false;
    }
  }
  return true;
}

uint8_t computePercent(int32_t remainGramsX100, int32_t capacityGramsX100) {
  if (capacityGramsX100 <= 0 || remainGramsX100 <= 0) {
    return 0;
  }
  int32_t percent = (remainGramsX100 * 100) / capacityGramsX100;
  if (percent > 100) {
    percent = 100;
  }
  return static_cast<uint8_t>(percent);
}

}  // namespace

FeederBucketCodecResult encodeFeederBucketSnapshot(const FeederBucketSnapshot& snapshot,
                                                   uint8_t* out,
                                                   std::size_t capacity,
                                                   std::size_t& encodedLength) {
  encodedLength = 0;
  if (out == nullptr || !validSnapshot(snapshot)) {
    return FeederBucketCodecResult::InvalidArgument;
  }
  if (capacity < kFeederBucketEncodedBytes) {
    return FeederBucketCodecResult::BufferTooSmall;
  }

  std::memset(out, 0, kFeederBucketEncodedBytes);
  writeU32(out + 0, kFeederBucketMagic);
  writeU16(out + 4, kFeederBucketSchemaVersion);
  out[6] = kFeederMaxChannels;

  for (uint8_t i = 0; i < kFeederMaxChannels; ++i) {
    const FeederBucketState& channel = snapshot.channels[i];
    uint8_t* bytes = out + kFeederBucketHeaderBytes + (i * kFeederBucketChannelBytes);
    bytes[0] = flagsForChannel(channel);
    writeI32(bytes + 4, channel.baseInfo.outputPulsesPerRev);
    writeI32(bytes + 8, channel.baseInfo.gramsPerRevX100);
    writeI32(bytes + 12, channel.baseInfo.capacityGramsX100);
    writeI32(bytes + 16, channel.remainGramsX100);
    writeU32(bytes + 20, channel.lastRefillUnixTime);
  }

  const uint32_t crc = Esp32At24cRecordStore::crc32IsoHdlc(
      out + kFeederBucketHeaderBytes, kFeederBucketEncodedBytes - kFeederBucketHeaderBytes);
  writeU32(out + 12, crc);
  encodedLength = kFeederBucketEncodedBytes;
  return FeederBucketCodecResult::Ok;
}

FeederBucketCodecResult verifyFeederBucketSnapshot(const uint8_t* data, std::size_t length) {
  if (data == nullptr) {
    return FeederBucketCodecResult::InvalidArgument;
  }
  if (length < kFeederBucketEncodedBytes) {
    return FeederBucketCodecResult::BufferTooSmall;
  }
  if (readU32(data + 0) != kFeederBucketMagic ||
      readU16(data + 4) != kFeederBucketSchemaVersion || data[6] != kFeederMaxChannels) {
    return FeederBucketCodecResult::UnsupportedVersion;
  }
  const uint32_t expectedCrc = readU32(data + 12);
  const uint32_t actualCrc = Esp32At24cRecordStore::crc32IsoHdlc(
      data + kFeederBucketHeaderBytes, kFeederBucketEncodedBytes - kFeederBucketHeaderBytes);
  return expectedCrc == actualCrc ? FeederBucketCodecResult::Ok
                                  : FeederBucketCodecResult::CrcMismatch;
}

FeederBucketCodecResult decodeFeederBucketSnapshot(const uint8_t* data,
                                                   std::size_t length,
                                                   FeederBucketSnapshot& out) {
  const FeederBucketCodecResult verifyResult = verifyFeederBucketSnapshot(data, length);
  if (verifyResult != FeederBucketCodecResult::Ok) {
    return verifyResult;
  }

  FeederBucketSnapshot decoded;
  for (uint8_t i = 0; i < kFeederMaxChannels; ++i) {
    const uint8_t* bytes = data + kFeederBucketHeaderBytes + (i * kFeederBucketChannelBytes);
    FeederBucketState& channel = decoded.channels[i];
    channel.baseInfo.enabled = (bytes[0] & kChannelFlagEnabled) != 0;
    channel.underflow = (bytes[0] & kChannelFlagUnderflow) != 0;
    channel.baseInfo.outputPulsesPerRev = readI32(bytes + 4);
    channel.baseInfo.gramsPerRevX100 = readI32(bytes + 8);
    channel.baseInfo.capacityGramsX100 = readI32(bytes + 12);
    channel.remainGramsX100 = readI32(bytes + 16);
    channel.lastRefillUnixTime = readU32(bytes + 20);
    channel.remainPercent =
        computePercent(channel.remainGramsX100, channel.baseInfo.capacityGramsX100);
  }
  if (!validSnapshot(decoded)) {
    return FeederBucketCodecResult::InvalidArgument;
  }

  out = decoded;
  return FeederBucketCodecResult::Ok;
}
