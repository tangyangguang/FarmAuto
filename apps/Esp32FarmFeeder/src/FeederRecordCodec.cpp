#include "FeederRecordCodec.h"

#include "Esp32At24cRecordStore.h"

namespace {

uint16_t readU16(const uint8_t* bytes) {
  return static_cast<uint16_t>(bytes[0]) | (static_cast<uint16_t>(bytes[1]) << 8);
}

uint32_t readU32(const uint8_t* bytes) {
  return static_cast<uint32_t>(bytes[0]) | (static_cast<uint32_t>(bytes[1]) << 8) |
         (static_cast<uint32_t>(bytes[2]) << 16) | (static_cast<uint32_t>(bytes[3]) << 24);
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

void writeI32(uint8_t* bytes, int32_t value) {
  writeU32(bytes, static_cast<uint32_t>(value));
}

void writeU64(uint8_t* bytes, uint64_t value) {
  for (uint8_t i = 0; i < 8; ++i) {
    bytes[i] = static_cast<uint8_t>((value >> (i * 8)) & 0xFFu);
  }
}

uint32_t headerCrcFor(const uint8_t* header) {
  uint8_t copy[kFeederRecordHeaderSize];
  for (std::size_t i = 0; i < kFeederRecordHeaderSize; ++i) {
    copy[i] = header[i];
  }
  writeU32(copy + 28, 0);
  return Esp32At24cRecordStore::crc32IsoHdlc(copy, sizeof(copy));
}

void encodePayload(const FeederRecord& record, uint8_t* payload) {
  writeU32(payload + 0, record.bootId);
  writeU32(payload + 4, record.uptimeSec);
  payload[8] = record.channel;
  payload[9] = record.requestedMask;
  payload[10] = record.successMask;
  payload[11] = record.busyMask;
  payload[12] = record.faultMask;
  payload[13] = record.skippedMask;
  payload[14] = 0;
  payload[15] = 0;
  writeI32(payload + 16, record.targetPulses);
  writeI32(payload + 20, record.estimatedGramsX100);
  writeI32(payload + 24, record.actualPulses);
}

}  // namespace

FeederRecordEncodeResult encodeFeederRecord(const FeederRecord& record,
                                            uint8_t* out,
                                            std::size_t capacity,
                                            std::size_t& encodedLength) {
  FeederRecordEncodeResult result;
  encodedLength = 0;
  if (out == nullptr) {
    result.result = FeederRecordCodecResult::InvalidArgument;
    return result;
  }
  if (capacity < kFeederRecordEncodedMaxBytes) {
    result.result = FeederRecordCodecResult::BufferTooSmall;
    return result;
  }

  uint8_t* header = out;
  uint8_t* payload = out + kFeederRecordHeaderSize;

  for (std::size_t i = 0; i < kFeederRecordEncodedMaxBytes; ++i) {
    out[i] = 0;
  }

  encodePayload(record, payload);
  result.payloadCrc =
      Esp32At24cRecordStore::crc32IsoHdlc(payload, kFeederRecordPayloadSize);

  writeU32(header + 0, kFeederRecordMagic);
  writeU16(header + 4, kFeederRecordSchemaVersion);
  writeU16(header + 6, static_cast<uint16_t>(kFeederRecordHeaderSize));
  header[8] = static_cast<uint8_t>(record.type);
  header[9] = static_cast<uint8_t>(record.result);
  writeU16(header + 10, static_cast<uint16_t>(kFeederRecordPayloadSize));
  writeU32(header + 12, record.sequence);
  writeU64(header + 16, record.unixTime);
  writeU32(header + 24, result.payloadCrc);
  result.headerCrc = headerCrcFor(header);
  writeU32(header + 28, result.headerCrc);

  encodedLength = kFeederRecordEncodedMaxBytes;
  result.bytesWritten = encodedLength;
  result.payloadLength = static_cast<uint16_t>(kFeederRecordPayloadSize);
  result.result = FeederRecordCodecResult::Ok;
  return result;
}

FeederRecordCodecResult verifyFeederEncodedRecord(const uint8_t* data, std::size_t length) {
  if (data == nullptr) {
    return FeederRecordCodecResult::InvalidArgument;
  }
  if (length < kFeederRecordHeaderSize) {
    return FeederRecordCodecResult::BufferTooSmall;
  }
  if (readU32(data + 0) != kFeederRecordMagic ||
      readU16(data + 4) != kFeederRecordSchemaVersion ||
      readU16(data + 6) != kFeederRecordHeaderSize) {
    return FeederRecordCodecResult::UnsupportedVersion;
  }

  const uint16_t payloadLength = readU16(data + 10);
  if (payloadLength != kFeederRecordPayloadSize ||
      length < kFeederRecordHeaderSize + payloadLength) {
    return FeederRecordCodecResult::BufferTooSmall;
  }

  const uint32_t storedPayloadCrc = readU32(data + 24);
  const uint32_t calculatedPayloadCrc =
      Esp32At24cRecordStore::crc32IsoHdlc(data + kFeederRecordHeaderSize, payloadLength);
  if (storedPayloadCrc != calculatedPayloadCrc) {
    return FeederRecordCodecResult::CrcMismatch;
  }

  const uint32_t storedHeaderCrc = readU32(data + 28);
  const uint32_t calculatedHeaderCrc = headerCrcFor(data);
  if (storedHeaderCrc != calculatedHeaderCrc) {
    return FeederRecordCodecResult::CrcMismatch;
  }

  return FeederRecordCodecResult::Ok;
}
