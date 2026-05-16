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

FeederResolvedTarget resolveFeederTarget(const FeederChannelBaseInfo& info,
                                         const FeederTargetRequest& request);
