#include <cassert>
#include <climits>

#include "FeederBucket.h"

int main() {
  FeederBucketService buckets;

  FeederChannelBaseInfo info;
  info.enabled = true;
  info.outputPulsesPerRev = 4320;
  info.gramsPerRevX100 = 7000;
  info.capacityGramsX100 = 500000;
  assert(buckets.updateBaseInfo(0, info) == FeederBucketResult::Ok);
  assert(buckets.enabledChannelMask() == 0b0001);

  FeederBucketSnapshot snapshot = buckets.snapshot();
  assert(snapshot.channels[0].baseInfo.capacityGramsX100 == 500000);
  assert(snapshot.channels[0].remainGramsX100 == 0);
  assert(snapshot.channels[0].remainPercent == 0);

  assert(buckets.markFull(0, 1000) == FeederBucketResult::Ok);
  snapshot = buckets.snapshot();
  assert(snapshot.channels[0].remainGramsX100 == 500000);
  assert(snapshot.channels[0].remainPercent == 100);
  assert(snapshot.channels[0].lastRefillUnixTime == 1000);

  assert(buckets.consume(0, 7000) == FeederBucketResult::Ok);
  snapshot = buckets.snapshot();
  assert(snapshot.channels[0].remainGramsX100 == 493000);
  assert(snapshot.channels[0].remainPercent == 98);

  assert(buckets.addFeed(0, 10000, 1200) == FeederBucketResult::Ok);
  snapshot = buckets.snapshot();
  assert(snapshot.channels[0].remainGramsX100 == 500000);
  assert(snapshot.channels[0].lastRefillUnixTime == 1200);

  assert(buckets.setRemaining(0, 250000, 1300) == FeederBucketResult::Ok);
  snapshot = buckets.snapshot();
  assert(snapshot.channels[0].remainGramsX100 == 250000);
  assert(snapshot.channels[0].remainPercent == 50);

  info.capacityGramsX100 = INT_MAX;
  assert(buckets.updateBaseInfo(0, info) == FeederBucketResult::Ok);
  assert(buckets.setRemaining(0, INT_MAX - 10, 1350) == FeederBucketResult::Ok);
  assert(buckets.addFeed(0, 1000, 1360) == FeederBucketResult::Ok);
  snapshot = buckets.snapshot();
  assert(snapshot.channels[0].remainGramsX100 == INT_MAX);
  assert(snapshot.channels[0].remainPercent == 100);
  info.capacityGramsX100 = 500000;
  assert(buckets.updateBaseInfo(0, info) == FeederBucketResult::Ok);
  assert(buckets.setRemaining(0, 250000, 1370) == FeederBucketResult::Ok);

  assert(buckets.consume(0, 300000) == FeederBucketResult::Underflow);
  snapshot = buckets.snapshot();
  assert(snapshot.channels[0].remainGramsX100 == 0);
  assert(snapshot.channels[0].underflow);

  assert(buckets.updateBaseInfo(4, info) == FeederBucketResult::InvalidArgument);

  info.enabled = false;
  assert(buckets.updateBaseInfo(0, info) == FeederBucketResult::Ok);
  assert(buckets.enabledChannelMask() == 0);

  info.enabled = true;
  info.gramsPerRevX100 = 0;
  assert(buckets.updateBaseInfo(0, info) == FeederBucketResult::Ok);
  assert(buckets.enabledChannelMask() == 0b0001);

  FeederBucketSnapshot persisted;
  persisted.channels[0].baseInfo.enabled = true;
  persisted.channels[0].baseInfo.outputPulsesPerRev = 4320;
  persisted.channels[0].baseInfo.gramsPerRevX100 = 7000;
  persisted.channels[0].baseInfo.capacityGramsX100 = 500000;
  persisted.channels[0].remainGramsX100 = 125000;
  persisted.channels[0].lastRefillUnixTime = 1800000000;
  persisted.channels[1].baseInfo.enabled = true;
  persisted.channels[1].baseInfo.outputPulsesPerRev = 4000;
  persisted.channels[1].baseInfo.gramsPerRevX100 = 6500;
  persisted.channels[1].baseInfo.capacityGramsX100 = 300000;
  persisted.channels[1].remainGramsX100 = 0;
  persisted.channels[1].underflow = true;
  assert(buckets.restore(persisted) == FeederBucketResult::Ok);
  snapshot = buckets.snapshot();
  assert(snapshot.channels[0].remainPercent == 25);
  assert(snapshot.channels[0].lastRefillUnixTime == 1800000000);
  assert(snapshot.channels[1].underflow);

  persisted.channels[0].remainGramsX100 = 600000;
  assert(buckets.restore(persisted) == FeederBucketResult::InvalidArgument);

  return 0;
}
