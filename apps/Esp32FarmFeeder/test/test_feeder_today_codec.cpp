#include <cassert>
#include <cstddef>
#include <cstdint>

#include "FeederTodayCodec.h"

int main() {
  FeederTodaySnapshot snapshot;
  snapshot.serviceDate = 20260517;
  snapshot.channels[0].pulses = 4320;
  snapshot.channels[0].gramsX100 = 7000;
  snapshot.channels[1].pulses = 4000;
  snapshot.channels[1].gramsX100 = 6500;

  uint8_t bytes[kFeederTodayEncodedBytes] = {};
  std::size_t encodedLength = 0;
  assert(encodeFeederTodaySnapshot(snapshot, bytes, sizeof(bytes), encodedLength) ==
         FeederTodayCodecResult::Ok);
  assert(encodedLength == kFeederTodayEncodedBytes);
  assert(verifyFeederTodaySnapshot(bytes, encodedLength) == FeederTodayCodecResult::Ok);

  FeederTodaySnapshot decoded;
  assert(decodeFeederTodaySnapshot(bytes, encodedLength, decoded) ==
         FeederTodayCodecResult::Ok);
  assert(decoded.serviceDate == 20260517);
  assert(decoded.channels[0].pulses == 4320);
  assert(decoded.channels[0].gramsX100 == 7000);
  assert(decoded.channels[1].pulses == 4000);
  assert(decoded.channels[1].gramsX100 == 6500);

  bytes[kFeederTodayHeaderBytes] ^= 0x80;
  assert(verifyFeederTodaySnapshot(bytes, encodedLength) == FeederTodayCodecResult::CrcMismatch);

  snapshot.channels[0].gramsX100 = -1;
  assert(encodeFeederTodaySnapshot(snapshot, bytes, sizeof(bytes), encodedLength) ==
         FeederTodayCodecResult::InvalidArgument);

  return 0;
}
