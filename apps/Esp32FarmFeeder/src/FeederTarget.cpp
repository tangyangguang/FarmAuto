#include "FeederTarget.h"

namespace {

int32_t divRoundNearest(int64_t numerator, int64_t denominator) {
  if (denominator <= 0) {
    return 0;
  }
  return static_cast<int32_t>((numerator + denominator / 2) / denominator);
}

}  // namespace

FeederResolvedTarget resolveFeederTarget(const FeederChannelBaseInfo& info,
                                         const FeederTargetRequest& request) {
  FeederResolvedTarget resolved;
  if (!info.enabled || info.outputPulsesPerRev <= 0) {
    resolved.result = FeederTargetResult::InvalidArgument;
    return resolved;
  }

  if (request.mode == FeederTargetMode::Grams) {
    if (request.targetGramsX100 <= 0) {
      resolved.result = FeederTargetResult::InvalidArgument;
      return resolved;
    }
    if (info.gramsPerRevX100 <= 0) {
      resolved.result = FeederTargetResult::NotCalibrated;
      return resolved;
    }
    resolved.targetPulses = divRoundNearest(
        static_cast<int64_t>(request.targetGramsX100) * info.outputPulsesPerRev,
        info.gramsPerRevX100);
    resolved.estimatedGramsX100 = request.targetGramsX100;
    resolved.result = FeederTargetResult::Ok;
    return resolved;
  }

  if (request.mode == FeederTargetMode::Revolutions) {
    if (request.targetRevolutionsX100 <= 0) {
      resolved.result = FeederTargetResult::InvalidArgument;
      return resolved;
    }
    resolved.targetPulses = divRoundNearest(
        static_cast<int64_t>(request.targetRevolutionsX100) * info.outputPulsesPerRev,
        100);
    if (info.gramsPerRevX100 > 0) {
      resolved.estimatedGramsX100 = divRoundNearest(
          static_cast<int64_t>(request.targetRevolutionsX100) * info.gramsPerRevX100,
          100);
    }
    resolved.result = FeederTargetResult::Ok;
    return resolved;
  }

  resolved.result = FeederTargetResult::InvalidArgument;
  return resolved;
}

FeederTargetBatch resolveFeederTargetsForMask(const FeederBucketSnapshot& buckets,
                                              const FeederTargetSnapshot& targets,
                                              uint8_t requestedMask) {
  FeederTargetBatch batch;
  for (uint8_t i = 0; i < kFeederMaxChannels; ++i) {
    const uint8_t bit = static_cast<uint8_t>(1U << i);
    if ((requestedMask & bit) == 0) {
      continue;
    }

    batch.channels[i] = resolveFeederTarget(buckets.channels[i].baseInfo, targets.channels[i]);
    if (batch.channels[i].result == FeederTargetResult::Ok) {
      batch.okMask |= bit;
    } else if (batch.channels[i].result == FeederTargetResult::NotCalibrated) {
      batch.notCalibratedMask |= bit;
    } else {
      batch.invalidMask |= bit;
    }
  }
  return batch;
}

FeederTargetResult FeederTargetService::setTarget(uint8_t channelIndex,
                                                  const FeederTargetRequest& request) {
  if (!validChannel(channelIndex) || !validRequest(request)) {
    return FeederTargetResult::InvalidArgument;
  }
  FeederTargetRequest& target = snapshot_.channels[channelIndex];
  target.mode = request.mode;
  if (request.targetGramsX100 > 0) {
    target.targetGramsX100 = request.targetGramsX100;
  }
  if (request.targetRevolutionsX100 > 0) {
    target.targetRevolutionsX100 = request.targetRevolutionsX100;
  }
  return FeederTargetResult::Ok;
}

FeederTargetSnapshot FeederTargetService::snapshot() const {
  return snapshot_;
}

bool FeederTargetService::validChannel(uint8_t channelIndex) {
  return channelIndex < kFeederMaxChannels;
}

bool FeederTargetService::validRequest(const FeederTargetRequest& request) {
  if (request.mode == FeederTargetMode::Grams) {
    return request.targetGramsX100 > 0;
  }
  if (request.mode == FeederTargetMode::Revolutions) {
    return request.targetRevolutionsX100 > 0;
  }
  return false;
}
