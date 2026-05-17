#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include <Esp32At24cRecordStore.h>

#include "FeederCalibrationCodec.h"

namespace {

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

}  // namespace

int main() {
  FeederBucketSnapshot snapshot;
  snapshot.channels[0].baseInfo.enabled = true;
  snapshot.channels[0].baseInfo.outputPulsesPerRev = 4320;
  snapshot.channels[0].baseInfo.gramsPerRevX100 = 7000;
  snapshot.channels[0].baseInfo.capacityGramsX100 = 500000;
  std::strncpy(snapshot.channels[0].baseInfo.name, "左侧料桶",
               sizeof(snapshot.channels[0].baseInfo.name) - 1);
  snapshot.channels[0].remainGramsX100 = 125000;
  snapshot.channels[0].underflow = true;
  snapshot.channels[1].baseInfo.enabled = true;
  snapshot.channels[1].baseInfo.outputPulsesPerRev = 4000;
  snapshot.channels[1].baseInfo.gramsPerRevX100 = 6500;
  snapshot.channels[1].baseInfo.capacityGramsX100 = 300000;
  std::strncpy(snapshot.channels[1].baseInfo.name, "右侧料桶",
               sizeof(snapshot.channels[1].baseInfo.name) - 1);

  uint8_t encoded[kFeederCalibrationEncodedBytes] = {};
  std::size_t encodedLength = 0;
  assert(encodeFeederCalibrationSnapshot(snapshot, encoded, sizeof(encoded), encodedLength) ==
         FeederCalibrationCodecResult::Ok);
  assert(encodedLength == kFeederCalibrationEncodedBytes);
  assert(verifyFeederCalibrationSnapshot(encoded, encodedLength) ==
         FeederCalibrationCodecResult::Ok);

  FeederBucketSnapshot decoded;
  assert(decodeFeederCalibrationSnapshot(encoded, encodedLength, decoded) ==
         FeederCalibrationCodecResult::Ok);
  assert(decoded.channels[0].baseInfo.enabled);
  assert(decoded.channels[0].baseInfo.outputPulsesPerRev == 4320);
  assert(decoded.channels[0].baseInfo.gramsPerRevX100 == 7000);
  assert(decoded.channels[0].baseInfo.capacityGramsX100 == 500000);
  assert(std::strcmp(decoded.channels[0].baseInfo.name, "左侧料桶") == 0);
  assert(decoded.channels[0].remainGramsX100 == 0);
  assert(!decoded.channels[0].underflow);
  assert(decoded.channels[1].baseInfo.enabled);
  assert(decoded.channels[1].baseInfo.outputPulsesPerRev == 4000);
  assert(std::strcmp(decoded.channels[1].baseInfo.name, "右侧料桶") == 0);

  uint8_t v1[kFeederCalibrationV1EncodedBytes] = {};
  writeU32(v1 + 0, kFeederCalibrationMagic);
  writeU16(v1 + 4, 1);
  v1[6] = kFeederMaxChannels;
  uint8_t* firstV1Channel = v1 + kFeederCalibrationHeaderBytes;
  firstV1Channel[0] = 0x01;
  writeI32(firstV1Channel + 4, 4320);
  writeI32(firstV1Channel + 8, 7000);
  writeI32(firstV1Channel + 12, 500000);
  const uint32_t v1Crc = Esp32At24cRecordStore::crc32IsoHdlc(
      v1 + kFeederCalibrationHeaderBytes,
      kFeederCalibrationV1EncodedBytes - kFeederCalibrationHeaderBytes);
  writeU32(v1 + 12, v1Crc);
  FeederBucketSnapshot decodedV1;
  assert(decodeFeederCalibrationSnapshot(v1, sizeof(v1), decodedV1) ==
         FeederCalibrationCodecResult::Ok);
  assert(decodedV1.channels[0].baseInfo.outputPulsesPerRev == 4320);
  assert(std::strcmp(decodedV1.channels[0].baseInfo.name, "通道 1") == 0);

  encoded[12] ^= 0xFF;
  assert(verifyFeederCalibrationSnapshot(encoded, encodedLength) ==
         FeederCalibrationCodecResult::CrcMismatch);

  snapshot.channels[0].baseInfo.capacityGramsX100 = -1;
  assert(encodeFeederCalibrationSnapshot(snapshot, encoded, sizeof(encoded), encodedLength) ==
         FeederCalibrationCodecResult::InvalidArgument);

  return 0;
}
