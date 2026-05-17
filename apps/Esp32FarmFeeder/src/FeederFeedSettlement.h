#pragma once

#include <cstdint>

#include "FeederBucket.h"
#include "FeederRunTracker.h"
#include "FeederToday.h"

enum class FeederFeedSettlementResult : uint8_t {
  Ok,
  Underflow,
  InvalidArgument
};

struct FeederFeedSettlement {
  int32_t actualPulses = 0;
  int32_t actualGramsX100 = 0;
};

FeederFeedSettlementResult settleCompletedFeederRun(uint8_t channelIndex,
                                                    const FeederRunChannel& completed,
                                                    FeederTodayService& today,
                                                    FeederBucketService& buckets,
                                                    FeederFeedSettlement& out);
