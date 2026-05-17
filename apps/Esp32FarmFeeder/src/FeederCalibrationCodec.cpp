#include "FeederCalibrationCodec.h"

#include <Esp32At24cRecordStore.h>

#include <cstring>

namespace {

static constexpr uint8_t kChannelFlagEnabled = 0x01;

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

bool emptyBaseInfo(const FeederChannelBaseInfo& info) {
  return !info.enabled && info.outputPulsesPerRev == 0 && info.gramsPerRevX100 == 0 &&
         info.capacityGramsX100 == 0;
}

bool validBaseInfo(const FeederChannelBaseInfo& info) {
  if (emptyBaseInfo(info)) {
    return true;
  }
  return info.outputPulsesPerRev > 0 && info.gramsPerRevX100 >= 0 &&
         info.capacityGramsX100 > 0;
}

bool validSnapshot(const FeederBucketSnapshot& snapshot) {
  for (uint8_t i = 0; i < kFeederMaxChannels; ++i) {
    if (!validBaseInfo(snapshot.channels[i].baseInfo)) {
      return false;
    }
  }
  return true;
}

}  // namespace

FeederCalibrationCodecResult encodeFeederCalibrationSnapshot(const FeederBucketSnapshot& snapshot,
                                                             uint8_t* out,
                                                             std::size_t capacity,
                                                             std::size_t& encodedLength) {
  encodedLength = 0;
  if (out == nullptr || !validSnapshot(snapshot)) {
    return FeederCalibrationCodecResult::InvalidArgument;
  }
  if (capacity < kFeederCalibrationEncodedBytes) {
    return FeederCalibrationCodecResult::BufferTooSmall;
  }

  std::memset(out, 0, kFeederCalibrationEncodedBytes);
  writeU32(out + 0, kFeederCalibrationMagic);
  writeU16(out + 4, kFeederCalibrationSchemaVersion);
  out[6] = kFeederMaxChannels;

  for (uint8_t i = 0; i < kFeederMaxChannels; ++i) {
    const FeederChannelBaseInfo& info = snapshot.channels[i].baseInfo;
    uint8_t* bytes = out + kFeederCalibrationHeaderBytes + (i * kFeederCalibrationChannelBytes);
    bytes[0] = info.enabled ? kChannelFlagEnabled : 0;
    writeI32(bytes + 4, info.outputPulsesPerRev);
    writeI32(bytes + 8, info.gramsPerRevX100);
    writeI32(bytes + 12, info.capacityGramsX100);
  }

  const uint32_t crc = Esp32At24cRecordStore::crc32IsoHdlc(
      out + kFeederCalibrationHeaderBytes,
      kFeederCalibrationEncodedBytes - kFeederCalibrationHeaderBytes);
  writeU32(out + 12, crc);
  encodedLength = kFeederCalibrationEncodedBytes;
  return FeederCalibrationCodecResult::Ok;
}

FeederCalibrationCodecResult verifyFeederCalibrationSnapshot(const uint8_t* data,
                                                             std::size_t length) {
  if (data == nullptr) {
    return FeederCalibrationCodecResult::InvalidArgument;
  }
  if (length < kFeederCalibrationEncodedBytes) {
    return FeederCalibrationCodecResult::BufferTooSmall;
  }
  if (readU32(data + 0) != kFeederCalibrationMagic ||
      readU16(data + 4) != kFeederCalibrationSchemaVersion ||
      data[6] != kFeederMaxChannels) {
    return FeederCalibrationCodecResult::UnsupportedVersion;
  }
  const uint32_t expectedCrc = readU32(data + 12);
  const uint32_t actualCrc = Esp32At24cRecordStore::crc32IsoHdlc(
      data + kFeederCalibrationHeaderBytes,
      kFeederCalibrationEncodedBytes - kFeederCalibrationHeaderBytes);
  return expectedCrc == actualCrc ? FeederCalibrationCodecResult::Ok
                                  : FeederCalibrationCodecResult::CrcMismatch;
}

FeederCalibrationCodecResult decodeFeederCalibrationSnapshot(const uint8_t* data,
                                                             std::size_t length,
                                                             FeederBucketSnapshot& out) {
  const FeederCalibrationCodecResult verifyResult = verifyFeederCalibrationSnapshot(data, length);
  if (verifyResult != FeederCalibrationCodecResult::Ok) {
    return verifyResult;
  }

  FeederBucketSnapshot decoded;
  for (uint8_t i = 0; i < kFeederMaxChannels; ++i) {
    const uint8_t* bytes = data + kFeederCalibrationHeaderBytes + (i * kFeederCalibrationChannelBytes);
    FeederChannelBaseInfo& info = decoded.channels[i].baseInfo;
    info.enabled = (bytes[0] & kChannelFlagEnabled) != 0;
    info.outputPulsesPerRev = readI32(bytes + 4);
    info.gramsPerRevX100 = readI32(bytes + 8);
    info.capacityGramsX100 = readI32(bytes + 12);
  }
  if (!validSnapshot(decoded)) {
    return FeederCalibrationCodecResult::InvalidArgument;
  }
  out = decoded;
  return FeederCalibrationCodecResult::Ok;
}
