#include "FeederTargetCodec.h"

#include <Esp32At24cRecordStore.h>

#include <cstring>

namespace {

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

bool emptyTarget(const FeederTargetRequest& request) {
  return request.mode == FeederTargetMode::None && request.targetGramsX100 == 0 &&
         request.targetRevolutionsX100 == 0;
}

bool validTarget(const FeederTargetRequest& request) {
  if (emptyTarget(request)) {
    return true;
  }
  if (request.mode == FeederTargetMode::Grams) {
    return request.targetGramsX100 > 0;
  }
  if (request.mode == FeederTargetMode::Revolutions) {
    return request.targetRevolutionsX100 > 0;
  }
  return false;
}

bool validSnapshot(const FeederTargetSnapshot& snapshot) {
  for (uint8_t i = 0; i < kFeederMaxChannels; ++i) {
    if (!validTarget(snapshot.channels[i])) {
      return false;
    }
  }
  return true;
}

}  // namespace

FeederTargetCodecResult encodeFeederTargetSnapshot(const FeederTargetSnapshot& snapshot,
                                                   uint8_t* out,
                                                   std::size_t capacity,
                                                   std::size_t& encodedLength) {
  encodedLength = 0;
  if (out == nullptr || !validSnapshot(snapshot)) {
    return FeederTargetCodecResult::InvalidArgument;
  }
  if (capacity < kFeederTargetEncodedBytes) {
    return FeederTargetCodecResult::BufferTooSmall;
  }

  std::memset(out, 0, kFeederTargetEncodedBytes);
  writeU32(out + 0, kFeederTargetMagic);
  writeU16(out + 4, kFeederTargetSchemaVersion);
  out[6] = kFeederMaxChannels;

  for (uint8_t i = 0; i < kFeederMaxChannels; ++i) {
    const FeederTargetRequest& target = snapshot.channels[i];
    uint8_t* bytes = out + kFeederTargetHeaderBytes + (i * kFeederTargetChannelBytes);
    bytes[0] = static_cast<uint8_t>(target.mode);
    writeI32(bytes + 4, target.targetGramsX100);
    writeI32(bytes + 8, target.targetRevolutionsX100);
  }

  const uint32_t crc = Esp32At24cRecordStore::crc32IsoHdlc(
      out + kFeederTargetHeaderBytes, kFeederTargetEncodedBytes - kFeederTargetHeaderBytes);
  writeU32(out + 12, crc);
  encodedLength = kFeederTargetEncodedBytes;
  return FeederTargetCodecResult::Ok;
}

FeederTargetCodecResult verifyFeederTargetSnapshot(const uint8_t* data, std::size_t length) {
  if (data == nullptr) {
    return FeederTargetCodecResult::InvalidArgument;
  }
  if (length < kFeederTargetEncodedBytes) {
    return FeederTargetCodecResult::BufferTooSmall;
  }
  if (readU32(data + 0) != kFeederTargetMagic ||
      readU16(data + 4) != kFeederTargetSchemaVersion || data[6] != kFeederMaxChannels) {
    return FeederTargetCodecResult::UnsupportedVersion;
  }
  const uint32_t expectedCrc = readU32(data + 12);
  const uint32_t actualCrc = Esp32At24cRecordStore::crc32IsoHdlc(
      data + kFeederTargetHeaderBytes, kFeederTargetEncodedBytes - kFeederTargetHeaderBytes);
  return expectedCrc == actualCrc ? FeederTargetCodecResult::Ok
                                  : FeederTargetCodecResult::CrcMismatch;
}

FeederTargetCodecResult decodeFeederTargetSnapshot(const uint8_t* data,
                                                   std::size_t length,
                                                   FeederTargetSnapshot& out) {
  const FeederTargetCodecResult verifyResult = verifyFeederTargetSnapshot(data, length);
  if (verifyResult != FeederTargetCodecResult::Ok) {
    return verifyResult;
  }

  FeederTargetSnapshot decoded;
  for (uint8_t i = 0; i < kFeederMaxChannels; ++i) {
    const uint8_t* bytes = data + kFeederTargetHeaderBytes + (i * kFeederTargetChannelBytes);
    FeederTargetRequest& target = decoded.channels[i];
    target.mode = static_cast<FeederTargetMode>(bytes[0]);
    target.targetGramsX100 = readI32(bytes + 4);
    target.targetRevolutionsX100 = readI32(bytes + 8);
  }
  if (!validSnapshot(decoded)) {
    return FeederTargetCodecResult::InvalidArgument;
  }

  out = decoded;
  return FeederTargetCodecResult::Ok;
}
