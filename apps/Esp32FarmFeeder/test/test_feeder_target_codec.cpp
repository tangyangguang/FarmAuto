#include <cassert>
#include <cstddef>
#include <cstdint>

#include "FeederTargetCodec.h"

int main() {
  FeederTargetSnapshot snapshot;
  snapshot.channels[0].mode = FeederTargetMode::Grams;
  snapshot.channels[0].targetGramsX100 = 5000;
  snapshot.channels[1].mode = FeederTargetMode::Revolutions;
  snapshot.channels[1].targetRevolutionsX100 = 125;

  uint8_t encoded[kFeederTargetEncodedBytes] = {};
  std::size_t encodedLength = 0;
  assert(encodeFeederTargetSnapshot(snapshot, encoded, sizeof(encoded), encodedLength) ==
         FeederTargetCodecResult::Ok);
  assert(encodedLength == kFeederTargetEncodedBytes);
  assert(verifyFeederTargetSnapshot(encoded, encodedLength) == FeederTargetCodecResult::Ok);

  FeederTargetSnapshot decoded;
  assert(decodeFeederTargetSnapshot(encoded, encodedLength, decoded) ==
         FeederTargetCodecResult::Ok);
  assert(decoded.channels[0].mode == FeederTargetMode::Grams);
  assert(decoded.channels[0].targetGramsX100 == 5000);
  assert(decoded.channels[1].mode == FeederTargetMode::Revolutions);
  assert(decoded.channels[1].targetRevolutionsX100 == 125);

  encoded[12] ^= 0xFF;
  assert(verifyFeederTargetSnapshot(encoded, encodedLength) == FeederTargetCodecResult::CrcMismatch);

  snapshot.channels[2].mode = FeederTargetMode::Grams;
  snapshot.channels[2].targetGramsX100 = 0;
  assert(encodeFeederTargetSnapshot(snapshot, encoded, sizeof(encoded), encodedLength) ==
         FeederTargetCodecResult::InvalidArgument);

  return 0;
}
