#include <cassert>
#include <cstddef>
#include <cstdint>

#include "FeederCalibrationCodec.h"

int main() {
  FeederBucketSnapshot snapshot;
  snapshot.channels[0].baseInfo.enabled = true;
  snapshot.channels[0].baseInfo.outputPulsesPerRev = 4320;
  snapshot.channels[0].baseInfo.gramsPerRevX100 = 7000;
  snapshot.channels[0].baseInfo.capacityGramsX100 = 500000;
  snapshot.channels[0].remainGramsX100 = 125000;
  snapshot.channels[0].underflow = true;
  snapshot.channels[1].baseInfo.enabled = true;
  snapshot.channels[1].baseInfo.outputPulsesPerRev = 4000;
  snapshot.channels[1].baseInfo.gramsPerRevX100 = 6500;
  snapshot.channels[1].baseInfo.capacityGramsX100 = 300000;

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
  assert(decoded.channels[0].remainGramsX100 == 0);
  assert(!decoded.channels[0].underflow);
  assert(decoded.channels[1].baseInfo.enabled);
  assert(decoded.channels[1].baseInfo.outputPulsesPerRev == 4000);

  encoded[12] ^= 0xFF;
  assert(verifyFeederCalibrationSnapshot(encoded, encodedLength) ==
         FeederCalibrationCodecResult::CrcMismatch);

  snapshot.channels[0].baseInfo.capacityGramsX100 = -1;
  assert(encodeFeederCalibrationSnapshot(snapshot, encoded, sizeof(encoded), encodedLength) ==
         FeederCalibrationCodecResult::InvalidArgument);

  return 0;
}
