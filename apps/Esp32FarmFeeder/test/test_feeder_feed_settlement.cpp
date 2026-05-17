#include <cassert>

#include "FeederBucket.h"
#include "FeederFeedSettlement.h"
#include "FeederToday.h"

int main() {
  FeederBucketService buckets;
  FeederChannelBaseInfo info;
  info.enabled = true;
  info.outputPulsesPerRev = 4320;
  info.gramsPerRevX100 = 7000;
  info.capacityGramsX100 = 500000;
  assert(buckets.updateBaseInfo(0, info) == FeederBucketResult::Ok);
  assert(buckets.setRemaining(0, 100000, 1000) == FeederBucketResult::Ok);

  FeederTodayService today;
  today.beginDay(20260517);

  FeederRunChannel completed;
  completed.active = true;
  completed.targetPulses = 4320;
  completed.estimatedGramsX100 = 7000;
  completed.actualPulses = 2160;

  FeederFeedSettlement settlement;
  assert(settleCompletedFeederRun(0, completed, today, buckets, settlement) ==
         FeederFeedSettlementResult::Ok);
  assert(settlement.actualGramsX100 == 3500);
  assert(settlement.actualPulses == 2160);

  FeederTodaySnapshot todaySnapshot = today.snapshot();
  assert(todaySnapshot.channels[0].pulses == 2160);
  assert(todaySnapshot.channels[0].gramsX100 == 3500);

  FeederBucketSnapshot bucketSnapshot = buckets.snapshot();
  assert(bucketSnapshot.channels[0].remainGramsX100 == 96500);
  assert(!bucketSnapshot.channels[0].underflow);

  completed.actualPulses = 100000;
  assert(settleCompletedFeederRun(0, completed, today, buckets, settlement) ==
         FeederFeedSettlementResult::Underflow);
  bucketSnapshot = buckets.snapshot();
  assert(bucketSnapshot.channels[0].remainGramsX100 == 0);
  assert(bucketSnapshot.channels[0].underflow);

  completed.targetPulses = 0;
  assert(settleCompletedFeederRun(0, completed, today, buckets, settlement) ==
         FeederFeedSettlementResult::InvalidArgument);
  completed.targetPulses = 4320;
  completed.actualPulses = -1;
  assert(settleCompletedFeederRun(0, completed, today, buckets, settlement) ==
         FeederFeedSettlementResult::InvalidArgument);
  completed.actualPulses = 1;
  assert(settleCompletedFeederRun(4, completed, today, buckets, settlement) ==
         FeederFeedSettlementResult::InvalidArgument);

  return 0;
}
