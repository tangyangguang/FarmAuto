#include <cassert>
#include <cstddef>
#include <cstdint>

#include "FeederScheduleCodec.h"

int main() {
  FeederScheduleSnapshot snapshot;
  snapshot.serviceDate = 20260517;
  snapshot.planCount = 2;

  FeederPlanState& first = snapshot.plans[0];
  first.config.planId = 1;
  first.config.enabled = true;
  first.config.timeConfigured = true;
  first.config.timeMinutes = 7 * 60 + 30;
  first.config.channelMask = 0b0011;
  first.config.targets[0].mode = FeederTargetMode::Grams;
  first.config.targets[0].targetGramsX100 = 7000;
  first.config.targets[1].mode = FeederTargetMode::Revolutions;
  first.config.targets[1].targetRevolutionsX100 = 125;
  first.scheduleAttemptedToday = true;
  first.todayExecuted = true;

  FeederPlanState& second = snapshot.plans[1];
  second.config.planId = 2;
  second.config.enabled = true;
  second.config.timeConfigured = true;
  second.config.timeMinutes = 18 * 60;
  second.config.channelMask = 0b0001;
  second.config.targets[0].mode = FeederTargetMode::Grams;
  second.config.targets[0].targetGramsX100 = 9000;
  second.skipToday = true;
  second.skipServiceDate = snapshot.serviceDate;
  second.scheduleMissedToday = true;

  uint8_t encoded[kFeederScheduleEncodedBytes] = {};
  std::size_t encodedLength = 0;
  assert(encodeFeederScheduleSnapshot(snapshot, encoded, sizeof(encoded), encodedLength) ==
         FeederScheduleCodecResult::Ok);
  assert(encodedLength == kFeederScheduleEncodedBytes);
  assert(verifyFeederScheduleSnapshot(encoded, encodedLength) == FeederScheduleCodecResult::Ok);

  FeederScheduleSnapshot decoded;
  assert(decodeFeederScheduleSnapshot(encoded, encodedLength, decoded) ==
         FeederScheduleCodecResult::Ok);
  assert(decoded.serviceDate == snapshot.serviceDate);
  assert(decoded.planCount == 2);
  assert(decoded.plans[0].config.planId == 1);
  assert(decoded.plans[0].config.timeMinutes == 450);
  assert(decoded.plans[0].config.targets[0].targetGramsX100 == 7000);
  assert(decoded.plans[0].config.targets[1].targetRevolutionsX100 == 125);
  assert(decoded.plans[0].scheduleAttemptedToday);
  assert(decoded.plans[0].todayExecuted);
  assert(decoded.plans[1].skipToday);
  assert(decoded.plans[1].skipServiceDate == snapshot.serviceDate);
  assert(decoded.plans[1].scheduleMissedToday);

  encoded[12] ^= 0xFF;
  assert(verifyFeederScheduleSnapshot(encoded, encodedLength) ==
         FeederScheduleCodecResult::CrcMismatch);

  std::size_t smallLength = 0;
  assert(encodeFeederScheduleSnapshot(snapshot, encoded, kFeederScheduleEncodedBytes - 1, smallLength) ==
         FeederScheduleCodecResult::BufferTooSmall);

  snapshot.planCount = kFeederMaxPlans + 1;
  assert(encodeFeederScheduleSnapshot(snapshot, encoded, sizeof(encoded), encodedLength) ==
         FeederScheduleCodecResult::InvalidArgument);

  return 0;
}
