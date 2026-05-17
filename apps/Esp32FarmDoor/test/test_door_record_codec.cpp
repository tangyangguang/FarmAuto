#include <cassert>
#include <cstddef>
#include <cstdint>

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

}  // namespace

int main() {
  const uint8_t crcInput[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
  assert(Esp32At24cRecordStore::crc32IsoHdlc(crcInput, sizeof(crcInput)) == 0xCBF43926u);

  DoorRecord record;
  record.sequence = 12;
  record.unixTime = 1800001234;
  record.uptimeSec = 99;
  record.bootId = 4;
  record.commandId = 123456;
  record.type = DoorRecordType::TravelAdjusted;
  record.result = DoorRecordResult::Ok;
  record.command = DoorCommand::Open;
  record.oldPositionPulses = 100;
  record.newPositionPulses = 150;
  record.oldTravelPulses = 10480;
  record.newTravelPulses = 11004;
  record.deltaPulses = 123;

  uint8_t encoded[kDoorRecordEncodedMaxBytes] = {};
  std::size_t encodedLength = 0;
  DoorRecordEncodeResult result = encodeDoorRecord(record, encoded, sizeof(encoded), encodedLength);
  assert(result.result == DoorRecordCodecResult::Ok);
  assert(encodedLength == kDoorRecordHeaderSize + kDoorRecordPayloadSize);
  assert(result.bytesWritten == encodedLength);
  assert(result.payloadLength == kDoorRecordPayloadSize);

  assert(readU32(encoded + 0) == kDoorRecordMagic);
  assert(readU16(encoded + 4) == kDoorRecordSchemaVersion);
  assert(readU16(encoded + 6) == kDoorRecordHeaderSize);
  assert(encoded[8] == static_cast<uint8_t>(DoorRecordType::TravelAdjusted));
  assert(encoded[9] == static_cast<uint8_t>(DoorRecordResult::Ok));
  assert(readU16(encoded + 10) == kDoorRecordPayloadSize);
  assert(readU32(encoded + 12) == 12);
  assert(static_cast<uint32_t>(readU64(encoded + 16)) == 1800001234u);
  assert(static_cast<uint32_t>(readU64(encoded + 16) >> 32) == record.commandId);
  assert(verifyDoorEncodedRecord(encoded, encodedLength) == DoorRecordCodecResult::Ok);

  DoorRecord decoded;
  assert(decodeDoorEncodedRecord(encoded, encodedLength, decoded) == DoorRecordCodecResult::Ok);
  assert(decoded.sequence == record.sequence);
  assert(decoded.unixTime == record.unixTime);
  assert(decoded.uptimeSec == record.uptimeSec);
  assert(decoded.bootId == record.bootId);
  assert(decoded.commandId == record.commandId);
  assert(decoded.type == record.type);
  assert(decoded.result == record.result);
  assert(decoded.command == record.command);
  assert(decoded.oldPositionPulses == record.oldPositionPulses);
  assert(decoded.newPositionPulses == record.newPositionPulses);
  assert(decoded.oldTravelPulses == record.oldTravelPulses);
  assert(decoded.newTravelPulses == record.newTravelPulses);
  assert(decoded.deltaPulses == record.deltaPulses);

  encoded[encodedLength - 1] ^= 0x01u;
  assert(verifyDoorEncodedRecord(encoded, encodedLength) == DoorRecordCodecResult::CrcMismatch);
  assert(decodeDoorEncodedRecord(encoded, encodedLength, decoded) == DoorRecordCodecResult::CrcMismatch);

  uint8_t tooSmall[kDoorRecordHeaderSize + kDoorRecordPayloadSize - 1] = {};
  encodedLength = 99;
  result = encodeDoorRecord(record, tooSmall, sizeof(tooSmall), encodedLength);
  assert(result.result == DoorRecordCodecResult::BufferTooSmall);
  assert(encodedLength == 0);

  return 0;
}
