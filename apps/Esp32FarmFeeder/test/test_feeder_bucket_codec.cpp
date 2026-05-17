#include <cassert>
#include <cstddef>
#include <cstdint>

#include "FeederBucketCodec.h"

int main() {
  FeederBucketSnapshot snapshot;
  snapshot.channels[0].baseInfo.enabled = true;
  snapshot.channels[0].baseInfo.outputPulsesPerRev = 4320;
  snapshot.channels[0].baseInfo.gramsPerRevX100 = 7000;
  snapshot.channels[0].baseInfo.capacityGramsX100 = 500000;
  snapshot.channels[0].remainGramsX100 = 125000;
  snapshot.channels[0].lastRefillUnixTime = 1800000000;

  snapshot.channels[1].baseInfo.enabled = true;
  snapshot.channels[1].baseInfo.outputPulsesPerRev = 4000;
  snapshot.channels[1].baseInfo.gramsPerRevX100 = 6500;
  snapshot.channels[1].baseInfo.capacityGramsX100 = 300000;
  snapshot.channels[1].remainGramsX100 = 0;
  snapshot.channels[1].underflow = true;

  uint8_t encoded[kFeederBucketEncodedBytes] = {};
  std::size_t encodedLength = 0;
  assert(encodeFeederBucketSnapshot(snapshot, encoded, sizeof(encoded), encodedLength) ==
         FeederBucketCodecResult::Ok);
  assert(encodedLength == kFeederBucketEncodedBytes);
  assert(verifyFeederBucketSnapshot(encoded, encodedLength) == FeederBucketCodecResult::Ok);

  FeederBucketSnapshot decoded;
  assert(decodeFeederBucketSnapshot(encoded, encodedLength, decoded) ==
         FeederBucketCodecResult::Ok);
  assert(!decoded.channels[0].baseInfo.enabled);
  assert(decoded.channels[0].baseInfo.outputPulsesPerRev == 0);
  assert(decoded.channels[0].baseInfo.gramsPerRevX100 == 0);
  assert(decoded.channels[0].baseInfo.capacityGramsX100 == 0);
  assert(decoded.channels[0].remainGramsX100 == 125000);
  assert(decoded.channels[0].remainPercent == 0);
  assert(decoded.channels[0].lastRefillUnixTime == 1800000000);
  assert(decoded.channels[1].underflow);

  encoded[12] ^= 0xFF;
  assert(verifyFeederBucketSnapshot(encoded, encodedLength) == FeederBucketCodecResult::CrcMismatch);

  std::size_t smallLength = 0;
  assert(encodeFeederBucketSnapshot(snapshot, encoded, kFeederBucketEncodedBytes - 1, smallLength) ==
         FeederBucketCodecResult::BufferTooSmall);

  snapshot.channels[0].remainGramsX100 = 600000;
  assert(encodeFeederBucketSnapshot(snapshot, encoded, sizeof(encoded), encodedLength) ==
         FeederBucketCodecResult::InvalidArgument);

  return 0;
}
