#include <cassert>

#include "FeederBucket.h"

int main() {
  FeederBucketService buckets;

  FeederChannelBaseInfo info;
  info.enabled = true;
  info.outputPulsesPerRev = 4320;
  info.gramsPerRevX100 = 7000;
  info.capacityGramsX100 = 500000;
  assert(buckets.updateBaseInfo(0, info) == FeederBucketResult::Ok);

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

  assert(buckets.consume(0, 300000) == FeederBucketResult::Underflow);
  snapshot = buckets.snapshot();
  assert(snapshot.channels[0].remainGramsX100 == 0);
  assert(snapshot.channels[0].underflow);

  assert(buckets.updateBaseInfo(4, info) == FeederBucketResult::InvalidArgument);

  return 0;
}
