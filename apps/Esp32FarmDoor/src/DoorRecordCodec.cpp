#include "DoorRecordCodec.h"

#include "Esp32At24cRecordStore.h"

namespace {

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
  bytes[0] = static_cast<uint8_t>(value & 0xFFu);
  bytes[1] = static_cast<uint8_t>((value >> 8) & 0xFFu);
}

void writeU32(uint8_t* bytes, uint32_t value) {
  bytes[0] = static_cast<uint8_t>(value & 0xFFu);
  bytes[1] = static_cast<uint8_t>((value >> 8) & 0xFFu);
  bytes[2] = static_cast<uint8_t>((value >> 16) & 0xFFu);
  bytes[3] = static_cast<uint8_t>((value >> 24) & 0xFFu);
}

void writeU64(uint8_t* bytes, uint64_t value) {
  for (uint8_t i = 0; i < 8; ++i) {
    bytes[i] = static_cast<uint8_t>((value >> (i * 8)) & 0xFFu);
  }
}

void writeI64(uint8_t* bytes, int64_t value) {
  writeU64(bytes, static_cast<uint64_t>(value));
}

uint32_t headerCrcFor(const uint8_t* header) {
  uint8_t copy[kDoorRecordHeaderSize];
  for (std::size_t i = 0; i < kDoorRecordHeaderSize; ++i) {
    copy[i] = header[i];
  }
  writeU32(copy + 28, 0);
  return Esp32At24cRecordStore::crc32IsoHdlc(copy, sizeof(copy));
}

void encodePayload(const DoorRecord& record, uint8_t* payload) {
  writeU32(payload + 0, record.bootId);
  writeU32(payload + 4, record.uptimeSec);
  payload[8] = static_cast<uint8_t>(record.command);
  payload[9] = 0;
  payload[10] = 0;
  payload[11] = 0;
  writeI64(payload + 12, record.oldPositionPulses);
  writeI64(payload + 20, record.newPositionPulses);
  writeI64(payload + 28, record.oldTravelPulses);
  writeI64(payload + 36, record.newTravelPulses);
}

}  // namespace

DoorRecordEncodeResult encodeDoorRecord(const DoorRecord& record,
                                        uint8_t* out,
                                        std::size_t capacity,
                                        std::size_t& encodedLength) {
  DoorRecordEncodeResult result;
  encodedLength = 0;
  if (out == nullptr) {
    result.result = DoorRecordCodecResult::InvalidArgument;
    return result;
  }
  if (capacity < kDoorRecordEncodedMaxBytes) {
    result.result = DoorRecordCodecResult::BufferTooSmall;
    return result;
  }

  uint8_t* header = out;
  uint8_t* payload = out + kDoorRecordHeaderSize;
  for (std::size_t i = 0; i < kDoorRecordEncodedMaxBytes; ++i) {
    out[i] = 0;
  }

  encodePayload(record, payload);
  result.payloadCrc = Esp32At24cRecordStore::crc32IsoHdlc(payload, kDoorRecordPayloadSize);

  writeU32(header + 0, kDoorRecordMagic);
  writeU16(header + 4, kDoorRecordSchemaVersion);
  writeU16(header + 6, static_cast<uint16_t>(kDoorRecordHeaderSize));
  header[8] = static_cast<uint8_t>(record.type);
  header[9] = static_cast<uint8_t>(record.result);
  writeU16(header + 10, static_cast<uint16_t>(kDoorRecordPayloadSize));
  writeU32(header + 12, record.sequence);
  writeU64(header + 16, record.unixTime);
  writeU32(header + 24, result.payloadCrc);
  result.headerCrc = headerCrcFor(header);
  writeU32(header + 28, result.headerCrc);

  encodedLength = kDoorRecordEncodedMaxBytes;
  result.bytesWritten = encodedLength;
  result.payloadLength = static_cast<uint16_t>(kDoorRecordPayloadSize);
  result.result = DoorRecordCodecResult::Ok;
  return result;
}

DoorRecordCodecResult verifyDoorEncodedRecord(const uint8_t* data, std::size_t length) {
  if (data == nullptr) {
    return DoorRecordCodecResult::InvalidArgument;
  }
  if (length < kDoorRecordHeaderSize) {
    return DoorRecordCodecResult::BufferTooSmall;
  }
  if (readU32(data + 0) != kDoorRecordMagic ||
      readU16(data + 4) != kDoorRecordSchemaVersion ||
      readU16(data + 6) != kDoorRecordHeaderSize) {
    return DoorRecordCodecResult::UnsupportedVersion;
  }

  const uint16_t payloadLength = readU16(data + 10);
  if (payloadLength != kDoorRecordPayloadSize ||
      length < kDoorRecordHeaderSize + payloadLength) {
    return DoorRecordCodecResult::BufferTooSmall;
  }

  const uint32_t storedPayloadCrc = readU32(data + 24);
  const uint32_t calculatedPayloadCrc =
      Esp32At24cRecordStore::crc32IsoHdlc(data + kDoorRecordHeaderSize, payloadLength);
  if (storedPayloadCrc != calculatedPayloadCrc) {
    return DoorRecordCodecResult::CrcMismatch;
  }

  const uint32_t storedHeaderCrc = readU32(data + 28);
  const uint32_t calculatedHeaderCrc = headerCrcFor(data);
  if (storedHeaderCrc != calculatedHeaderCrc) {
    return DoorRecordCodecResult::CrcMismatch;
  }

  return DoorRecordCodecResult::Ok;
}

DoorRecordCodecResult decodeDoorEncodedRecord(const uint8_t* data,
                                              std::size_t length,
                                              DoorRecord& out) {
  const DoorRecordCodecResult verifyResult = verifyDoorEncodedRecord(data, length);
  if (verifyResult != DoorRecordCodecResult::Ok) {
    return verifyResult;
  }

  const uint8_t* payload = data + kDoorRecordHeaderSize;
  DoorRecord record;
  record.type = static_cast<DoorRecordType>(data[8]);
  record.result = static_cast<DoorRecordResult>(data[9]);
  record.sequence = readU32(data + 12);
  record.unixTime = static_cast<uint32_t>(readU64(data + 16));
  record.bootId = readU32(payload + 0);
  record.uptimeSec = readU32(payload + 4);
  record.command = static_cast<DoorCommand>(payload[8]);
  record.oldPositionPulses = readI64(payload + 12);
  record.newPositionPulses = readI64(payload + 20);
  record.oldTravelPulses = readI64(payload + 28);
  record.newTravelPulses = readI64(payload + 36);
  record.deltaPulses = record.newTravelPulses - record.oldTravelPulses;
  out = record;
  return DoorRecordCodecResult::Ok;
}
