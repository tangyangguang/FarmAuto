#include "FeederTodayCodec.h"

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

bool validSnapshot(const FeederTodaySnapshot& snapshot) {
  for (uint8_t i = 0; i < kFeederMaxChannels; ++i) {
    if (snapshot.channels[i].pulses < 0 || snapshot.channels[i].gramsX100 < 0) {
      return false;
    }
  }
  return true;
}

}  // namespace

FeederTodayCodecResult encodeFeederTodaySnapshot(const FeederTodaySnapshot& snapshot,
                                                 uint8_t* out,
                                                 std::size_t capacity,
                                                 std::size_t& encodedLength) {
  encodedLength = 0;
  if (out == nullptr || !validSnapshot(snapshot)) {
    return FeederTodayCodecResult::InvalidArgument;
  }
  if (capacity < kFeederTodayEncodedBytes) {
    return FeederTodayCodecResult::BufferTooSmall;
  }

  std::memset(out, 0, kFeederTodayEncodedBytes);
  writeU32(out + 0, kFeederTodayMagic);
  writeU16(out + 4, kFeederTodaySchemaVersion);
  out[6] = kFeederMaxChannels;
  writeU32(out + kFeederTodayHeaderBytes, snapshot.serviceDate);

  uint8_t* channelBase = out + kFeederTodayHeaderBytes + 4;
  for (uint8_t i = 0; i < kFeederMaxChannels; ++i) {
    uint8_t* bytes = channelBase + (i * kFeederTodayChannelBytes);
    writeI32(bytes + 0, snapshot.channels[i].pulses);
    writeI32(bytes + 4, snapshot.channels[i].gramsX100);
  }

  const uint32_t crc = Esp32At24cRecordStore::crc32IsoHdlc(
      out + kFeederTodayHeaderBytes, kFeederTodayEncodedBytes - kFeederTodayHeaderBytes);
  writeU32(out + 12, crc);
  encodedLength = kFeederTodayEncodedBytes;
  return FeederTodayCodecResult::Ok;
}

FeederTodayCodecResult verifyFeederTodaySnapshot(const uint8_t* data, std::size_t length) {
  if (data == nullptr) {
    return FeederTodayCodecResult::InvalidArgument;
  }
  if (length < kFeederTodayEncodedBytes) {
    return FeederTodayCodecResult::BufferTooSmall;
  }
  if (readU32(data + 0) != kFeederTodayMagic ||
      readU16(data + 4) != kFeederTodaySchemaVersion || data[6] != kFeederMaxChannels) {
    return FeederTodayCodecResult::UnsupportedVersion;
  }
  const uint32_t expectedCrc = readU32(data + 12);
  const uint32_t actualCrc = Esp32At24cRecordStore::crc32IsoHdlc(
      data + kFeederTodayHeaderBytes, kFeederTodayEncodedBytes - kFeederTodayHeaderBytes);
  return expectedCrc == actualCrc ? FeederTodayCodecResult::Ok
                                  : FeederTodayCodecResult::CrcMismatch;
}

FeederTodayCodecResult decodeFeederTodaySnapshot(const uint8_t* data,
                                                 std::size_t length,
                                                 FeederTodaySnapshot& out) {
  const FeederTodayCodecResult verifyResult = verifyFeederTodaySnapshot(data, length);
  if (verifyResult != FeederTodayCodecResult::Ok) {
    return verifyResult;
  }

  FeederTodaySnapshot decoded;
  decoded.serviceDate = readU32(data + kFeederTodayHeaderBytes);
  const uint8_t* channelBase = data + kFeederTodayHeaderBytes + 4;
  for (uint8_t i = 0; i < kFeederMaxChannels; ++i) {
    const uint8_t* bytes = channelBase + (i * kFeederTodayChannelBytes);
    decoded.channels[i].pulses = readI32(bytes + 0);
    decoded.channels[i].gramsX100 = readI32(bytes + 4);
  }
  if (!validSnapshot(decoded)) {
    return FeederTodayCodecResult::InvalidArgument;
  }

  out = decoded;
  return FeederTodayCodecResult::Ok;
}
