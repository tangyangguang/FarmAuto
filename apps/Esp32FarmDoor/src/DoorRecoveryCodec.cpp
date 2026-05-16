#include "DoorRecoveryCodec.h"

#include <Esp32At24cRecordStore.h>

#include <cstring>

namespace {

static constexpr std::size_t kPayloadOffset = 16;

uint16_t readU16(const uint8_t* bytes) {
  return static_cast<uint16_t>(bytes[0]) | (static_cast<uint16_t>(bytes[1]) << 8);
}

uint32_t readU32(const uint8_t* bytes) {
  return static_cast<uint32_t>(bytes[0]) | (static_cast<uint32_t>(bytes[1]) << 8) |
         (static_cast<uint32_t>(bytes[2]) << 16) | (static_cast<uint32_t>(bytes[3]) << 24);
}

uint64_t readU64(const uint8_t* bytes) {
  uint64_t value = 0;
  for (uint8_t i = 0; i < 8; ++i) {
    value |= static_cast<uint64_t>(bytes[i]) << (i * 8);
  }
  return value;
}

int64_t readI64(const uint8_t* bytes) {
  return static_cast<int64_t>(readU64(bytes));
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

void writeU64(uint8_t* bytes, uint64_t value) {
  for (uint8_t i = 0; i < 8; ++i) {
    bytes[i] = static_cast<uint8_t>((value >> (i * 8)) & 0xFF);
  }
}

void writeI64(uint8_t* bytes, int64_t value) {
  writeU64(bytes, static_cast<uint64_t>(value));
}

bool validRecoveryState(const DoorRecoveryState& state) {
  if (state.openTargetPulses <= state.closedPositionPulses || state.maxRunPulses <= 0 ||
      state.maxRunMs == 0) {
    return false;
  }
  if (state.positionTrustLevel != PositionTrustLevel::Trusted &&
      state.positionTrustLevel != PositionTrustLevel::Limited &&
      state.positionTrustLevel != PositionTrustLevel::Untrusted) {
    return false;
  }
  return true;
}

}  // namespace

DoorRecoveryCodecResult encodeDoorRecoveryState(const DoorRecoveryState& state,
                                                uint8_t* out,
                                                std::size_t capacity,
                                                std::size_t& encodedLength) {
  encodedLength = 0;
  if (out == nullptr || !validRecoveryState(state)) {
    return DoorRecoveryCodecResult::InvalidArgument;
  }
  if (capacity < kDoorRecoveryEncodedBytes) {
    return DoorRecoveryCodecResult::BufferTooSmall;
  }

  std::memset(out, 0, kDoorRecoveryEncodedBytes);
  writeU32(out + 0, kDoorRecoveryMagic);
  writeU16(out + 4, kDoorRecoverySchemaVersion);
  writeU16(out + 6, static_cast<uint16_t>(kDoorRecoveryEncodedBytes));
  out[8] = static_cast<uint8_t>(state.positionTrustLevel);
  out[9] = static_cast<uint8_t>(state.lastCommand);
  out[10] = static_cast<uint8_t>(state.lastStopReason);

  uint8_t* payload = out + kPayloadOffset;
  writeI64(payload + 0, state.positionPulses);
  writeI64(payload + 8, state.openTargetPulses);
  writeI64(payload + 16, state.closedPositionPulses);
  writeI64(payload + 24, state.maxRunPulses);
  writeU32(payload + 32, state.maxRunMs);
  writeU32(payload + 36, state.unixTime);
  writeU32(payload + 40, state.uptimeSec);
  writeU32(payload + 44, state.bootId);

  const uint32_t crc =
      Esp32At24cRecordStore::crc32IsoHdlc(out + kPayloadOffset,
                                          kDoorRecoveryEncodedBytes - kPayloadOffset);
  writeU32(out + 12, crc);
  encodedLength = kDoorRecoveryEncodedBytes;
  return DoorRecoveryCodecResult::Ok;
}

DoorRecoveryCodecResult verifyDoorRecoveryState(const uint8_t* data, std::size_t length) {
  if (data == nullptr) {
    return DoorRecoveryCodecResult::InvalidArgument;
  }
  if (length < kDoorRecoveryEncodedBytes) {
    return DoorRecoveryCodecResult::BufferTooSmall;
  }
  if (readU32(data + 0) != kDoorRecoveryMagic ||
      readU16(data + 4) != kDoorRecoverySchemaVersion ||
      readU16(data + 6) != kDoorRecoveryEncodedBytes) {
    return DoorRecoveryCodecResult::UnsupportedVersion;
  }
  const uint32_t expectedCrc = readU32(data + 12);
  const uint32_t actualCrc =
      Esp32At24cRecordStore::crc32IsoHdlc(data + kPayloadOffset,
                                          kDoorRecoveryEncodedBytes - kPayloadOffset);
  return expectedCrc == actualCrc ? DoorRecoveryCodecResult::Ok
                                  : DoorRecoveryCodecResult::CrcMismatch;
}

DoorRecoveryCodecResult decodeDoorRecoveryState(const uint8_t* data,
                                                std::size_t length,
                                                DoorRecoveryState& out) {
  const DoorRecoveryCodecResult verifyResult = verifyDoorRecoveryState(data, length);
  if (verifyResult != DoorRecoveryCodecResult::Ok) {
    return verifyResult;
  }

  DoorRecoveryState decoded;
  decoded.positionTrustLevel = static_cast<PositionTrustLevel>(data[8]);
  decoded.lastCommand = static_cast<DoorCommand>(data[9]);
  decoded.lastStopReason = static_cast<DoorStopReason>(data[10]);

  const uint8_t* payload = data + kPayloadOffset;
  decoded.positionPulses = readI64(payload + 0);
  decoded.openTargetPulses = readI64(payload + 8);
  decoded.closedPositionPulses = readI64(payload + 16);
  decoded.maxRunPulses = readI64(payload + 24);
  decoded.maxRunMs = readU32(payload + 32);
  decoded.unixTime = readU32(payload + 36);
  decoded.uptimeSec = readU32(payload + 40);
  decoded.bootId = readU32(payload + 44);

  if (!validRecoveryState(decoded)) {
    return DoorRecoveryCodecResult::InvalidArgument;
  }
  out = decoded;
  return DoorRecoveryCodecResult::Ok;
}
