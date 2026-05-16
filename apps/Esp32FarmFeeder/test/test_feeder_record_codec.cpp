#include <cassert>
#include <cstddef>
#include <cstdint>

#include "Esp32At24cRecordStore.h"
#include "FeederRecordCodec.h"

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

  FeederRecord record;
  record.sequence = 42;
  record.unixTime = 1800000000;
  record.uptimeSec = 123;
  record.bootId = 7;
  record.type = FeederRecordType::ScheduleTriggered;
  record.result = FeederRecordResult::Partial;
  record.planId = 3;
  record.channel = 1;
  record.requestedMask = 0b0111;
  record.successMask = 0b0011;
  record.busyMask = 0b0100;
  record.faultMask = 0;
  record.skippedMask = 0b1000;
  record.targetPulses = 4320;
  record.estimatedGramsX100 = 7000;
  record.actualPulses = 4200;

  uint8_t encoded[kFeederRecordEncodedMaxBytes] = {};
  std::size_t encodedLength = 0;
  FeederRecordEncodeResult result = encodeFeederRecord(record, encoded, sizeof(encoded), encodedLength);
  assert(result.result == FeederRecordCodecResult::Ok);
  assert(encodedLength == kFeederRecordHeaderSize + kFeederRecordPayloadSize);
  assert(result.bytesWritten == encodedLength);
  assert(result.payloadLength == kFeederRecordPayloadSize);

  assert(readU32(encoded + 0) == kFeederRecordMagic);
  assert(readU16(encoded + 4) == kFeederRecordSchemaVersion);
  assert(readU16(encoded + 6) == kFeederRecordHeaderSize);
  assert(encoded[8] == static_cast<uint8_t>(FeederRecordType::ScheduleTriggered));
  assert(encoded[9] == static_cast<uint8_t>(FeederRecordResult::Partial));
  assert(readU16(encoded + 10) == kFeederRecordPayloadSize);
  assert(readU32(encoded + 12) == 42);
  assert(readU64(encoded + 16) == 1800000000ull);

  const uint32_t payloadCrc = readU32(encoded + 24);
  const uint32_t headerCrc = readU32(encoded + 28);
  assert(payloadCrc == result.payloadCrc);
  assert(headerCrc == result.headerCrc);
  assert(verifyFeederEncodedRecord(encoded, encodedLength) == FeederRecordCodecResult::Ok);

  FeederRecord decoded;
  assert(decodeFeederEncodedRecord(encoded, encodedLength, decoded) == FeederRecordCodecResult::Ok);
  assert(decoded.sequence == record.sequence);
  assert(decoded.unixTime == record.unixTime);
  assert(decoded.uptimeSec == record.uptimeSec);
  assert(decoded.bootId == record.bootId);
  assert(decoded.type == record.type);
  assert(decoded.result == record.result);
  assert(decoded.planId == record.planId);
  assert(decoded.channel == record.channel);
  assert(decoded.requestedMask == record.requestedMask);
  assert(decoded.successMask == record.successMask);
  assert(decoded.busyMask == record.busyMask);
  assert(decoded.faultMask == record.faultMask);
  assert(decoded.skippedMask == record.skippedMask);
  assert(decoded.targetPulses == record.targetPulses);
  assert(decoded.estimatedGramsX100 == record.estimatedGramsX100);
  assert(decoded.actualPulses == record.actualPulses);

  encoded[encodedLength - 1] ^= 0x01u;
  assert(verifyFeederEncodedRecord(encoded, encodedLength) == FeederRecordCodecResult::CrcMismatch);
  assert(decodeFeederEncodedRecord(encoded, encodedLength, decoded) ==
         FeederRecordCodecResult::CrcMismatch);

  uint8_t tooSmall[kFeederRecordHeaderSize + kFeederRecordPayloadSize - 1] = {};
  encodedLength = 99;
  result = encodeFeederRecord(record, tooSmall, sizeof(tooSmall), encodedLength);
  assert(result.result == FeederRecordCodecResult::BufferTooSmall);
  assert(encodedLength == 0);

  return 0;
}
