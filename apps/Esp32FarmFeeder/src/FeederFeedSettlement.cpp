#include "FeederFeedSettlement.h"

#include <cstdint>

namespace {

bool validChannel(uint8_t channelIndex) {
  return channelIndex < kFeederConfiguredChannels;
}

bool validCompletedRun(const FeederRunChannel& completed) {
  return completed.active && completed.targetPulses > 0 && completed.estimatedGramsX100 >= 0 &&
         completed.actualPulses >= 0;
}

int32_t scaledGramsX100(const FeederRunChannel& completed) {
  const int64_t scaled = static_cast<int64_t>(completed.estimatedGramsX100) *
                         static_cast<int64_t>(completed.actualPulses);
  return static_cast<int32_t>(scaled / completed.targetPulses);
}

}  // namespace

FeederFeedSettlementResult settleCompletedFeederRun(uint8_t channelIndex,
                                                    const FeederRunChannel& completed,
                                                    FeederTodayService& today,
                                                    FeederBucketService& buckets,
                                                    FeederFeedSettlement& out) {
  out = FeederFeedSettlement();
  if (!validChannel(channelIndex) || !validCompletedRun(completed)) {
    return FeederFeedSettlementResult::InvalidArgument;
  }

  const int32_t actualGramsX100 = scaledGramsX100(completed);
  if (today.addChannelFeed(channelIndex, completed.actualPulses, actualGramsX100) !=
      FeederTodayResult::Ok) {
    return FeederFeedSettlementResult::InvalidArgument;
  }

  out.actualPulses = completed.actualPulses;
  out.actualGramsX100 = actualGramsX100;
  const FeederBucketResult bucketResult = buckets.consume(channelIndex, actualGramsX100);
  if (bucketResult == FeederBucketResult::Underflow) {
    return FeederFeedSettlementResult::Underflow;
  }
  if (bucketResult != FeederBucketResult::Ok) {
    return FeederFeedSettlementResult::InvalidArgument;
  }
  return FeederFeedSettlementResult::Ok;
}
