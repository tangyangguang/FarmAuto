#pragma once

#include <cstdint>

#include "FeederBucket.h"
#include "FeederSchedule.h"

enum class FeederTargetResult : uint8_t {
  Ok,
  InvalidArgument,
  NotCalibrated
};

struct FeederTargetRequest {
  FeederTargetMode mode = FeederTargetMode::None;
  int32_t targetGramsX100 = 0;
  int32_t targetRevolutionsX100 = 0;
};

struct FeederResolvedTarget {
  FeederTargetResult result = FeederTargetResult::InvalidArgument;
  int32_t targetPulses = 0;
  int32_t estimatedGramsX100 = 0;
};

struct FeederTargetSnapshot {
  FeederTargetRequest channels[kFeederMaxChannels];
};

struct FeederTargetBatch {
  uint8_t okMask = 0;
  uint8_t invalidMask = 0;
  uint8_t notCalibratedMask = 0;
  FeederResolvedTarget channels[kFeederMaxChannels];
};

class FeederTargetService {
 public:
  FeederTargetResult setTarget(uint8_t channelIndex, const FeederTargetRequest& request);
  FeederTargetSnapshot snapshot() const;

 private:
  static bool validChannel(uint8_t channelIndex);
  static bool validRequest(const FeederTargetRequest& request);

  FeederTargetSnapshot snapshot_;
};

FeederResolvedTarget resolveFeederTarget(const FeederChannelBaseInfo& info,
                                         const FeederTargetRequest& request);
FeederTargetBatch resolveFeederTargetsForMask(const FeederBucketSnapshot& buckets,
                                              const FeederTargetSnapshot& targets,
                                              uint8_t requestedMask);
