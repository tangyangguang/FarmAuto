#include "FeederBucketRestore.h"

FeederBucketResult restoreFeederBucketParts(FeederBucketService& buckets,
                                            const FeederBucketSnapshot& calibration,
                                            const FeederBucketSnapshot& dynamicState) {
  FeederBucketSnapshot merged;
  for (uint8_t i = 0; i < kFeederMaxChannels; ++i) {
    merged.channels[i].baseInfo = calibration.channels[i].baseInfo;
    merged.channels[i].remainGramsX100 = dynamicState.channels[i].remainGramsX100;
    merged.channels[i].lastRefillUnixTime = dynamicState.channels[i].lastRefillUnixTime;
    merged.channels[i].underflow = dynamicState.channels[i].underflow;
  }
  return buckets.restore(merged);
}
