#include <cassert>

#include "FeederBucketRestore.h"

int main() {
  FeederBucketSnapshot calibration;
  calibration.channels[0].baseInfo.enabled = true;
  calibration.channels[0].baseInfo.outputPulsesPerRev = 4320;
  calibration.channels[0].baseInfo.gramsPerRevX100 = 7000;
  calibration.channels[0].baseInfo.capacityGramsX100 = 500000;

  FeederBucketSnapshot dynamic;
  dynamic.channels[0].remainGramsX100 = 125000;
  dynamic.channels[0].lastRefillUnixTime = 1800000000;
  dynamic.channels[0].underflow = true;

  FeederBucketService buckets;
  assert(restoreFeederBucketParts(buckets, calibration, dynamic) == FeederBucketResult::Ok);
  FeederBucketSnapshot snapshot = buckets.snapshot();
  assert(snapshot.channels[0].baseInfo.outputPulsesPerRev == 4320);
  assert(snapshot.channels[0].baseInfo.gramsPerRevX100 == 7000);
  assert(snapshot.channels[0].baseInfo.capacityGramsX100 == 500000);
  assert(snapshot.channels[0].remainGramsX100 == 125000);
  assert(snapshot.channels[0].remainPercent == 25);
  assert(snapshot.channels[0].lastRefillUnixTime == 1800000000);
  assert(snapshot.channels[0].underflow);

  dynamic.channels[0].remainGramsX100 = 600000;
  dynamic.channels[0].underflow = true;
  assert(restoreFeederBucketParts(buckets, calibration, dynamic) == FeederBucketResult::Ok);
  snapshot = buckets.snapshot();
  assert(snapshot.channels[0].remainGramsX100 == 500000);
  assert(snapshot.channels[0].remainPercent == 100);
  assert(!snapshot.channels[0].underflow);

  calibration.channels[0].baseInfo.capacityGramsX100 = 0;
  assert(restoreFeederBucketParts(buckets, calibration, FeederBucketSnapshot()) ==
         FeederBucketResult::InvalidArgument);

  return 0;
}
