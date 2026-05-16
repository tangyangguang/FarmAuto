#include "FeederBucket.h"

FeederBucketResult FeederBucketService::updateBaseInfo(uint8_t channelIndex,
                                                       const FeederChannelBaseInfo& info) {
  if (!validChannel(channelIndex)) {
    return FeederBucketResult::InvalidArgument;
  }
  const FeederBucketResult validation = validateBaseInfo(info);
  if (validation != FeederBucketResult::Ok) {
    return validation;
  }

  snapshot_.channels[channelIndex].baseInfo = info;
  if (snapshot_.channels[channelIndex].remainGramsX100 > info.capacityGramsX100) {
    snapshot_.channels[channelIndex].remainGramsX100 = info.capacityGramsX100;
  }
  recomputePercent(channelIndex);
  return FeederBucketResult::Ok;
}

FeederBucketResult FeederBucketService::setRemaining(uint8_t channelIndex,
                                                     int32_t remainGramsX100,
                                                     uint32_t unixTime) {
  if (!validChannel(channelIndex) || remainGramsX100 < 0) {
    return FeederBucketResult::InvalidArgument;
  }
  const int32_t capacity = snapshot_.channels[channelIndex].baseInfo.capacityGramsX100;
  if (capacity <= 0) {
    return FeederBucketResult::InvalidArgument;
  }
  if (remainGramsX100 > capacity) {
    remainGramsX100 = capacity;
  }
  snapshot_.channels[channelIndex].remainGramsX100 = remainGramsX100;
  snapshot_.channels[channelIndex].lastRefillUnixTime = unixTime;
  snapshot_.channels[channelIndex].underflow = false;
  recomputePercent(channelIndex);
  return FeederBucketResult::Ok;
}

FeederBucketResult FeederBucketService::addFeed(uint8_t channelIndex,
                                                int32_t addedGramsX100,
                                                uint32_t unixTime) {
  if (!validChannel(channelIndex) || addedGramsX100 <= 0) {
    return FeederBucketResult::InvalidArgument;
  }
  const int32_t capacity = snapshot_.channels[channelIndex].baseInfo.capacityGramsX100;
  if (capacity <= 0) {
    return FeederBucketResult::InvalidArgument;
  }
  int32_t nextRemain = snapshot_.channels[channelIndex].remainGramsX100 + addedGramsX100;
  if (nextRemain > capacity) {
    nextRemain = capacity;
  }
  snapshot_.channels[channelIndex].remainGramsX100 = nextRemain;
  snapshot_.channels[channelIndex].lastRefillUnixTime = unixTime;
  snapshot_.channels[channelIndex].underflow = false;
  recomputePercent(channelIndex);
  return FeederBucketResult::Ok;
}

FeederBucketResult FeederBucketService::markFull(uint8_t channelIndex, uint32_t unixTime) {
  if (!validChannel(channelIndex)) {
    return FeederBucketResult::InvalidArgument;
  }
  const int32_t capacity = snapshot_.channels[channelIndex].baseInfo.capacityGramsX100;
  if (capacity <= 0) {
    return FeederBucketResult::InvalidArgument;
  }
  snapshot_.channels[channelIndex].remainGramsX100 = capacity;
  snapshot_.channels[channelIndex].lastRefillUnixTime = unixTime;
  snapshot_.channels[channelIndex].underflow = false;
  recomputePercent(channelIndex);
  return FeederBucketResult::Ok;
}

FeederBucketResult FeederBucketService::consume(uint8_t channelIndex, int32_t usedGramsX100) {
  if (!validChannel(channelIndex) || usedGramsX100 < 0) {
    return FeederBucketResult::InvalidArgument;
  }
  FeederBucketState& channel = snapshot_.channels[channelIndex];
  if (usedGramsX100 > channel.remainGramsX100) {
    channel.remainGramsX100 = 0;
    channel.underflow = true;
    recomputePercent(channelIndex);
    return FeederBucketResult::Underflow;
  }
  channel.remainGramsX100 -= usedGramsX100;
  recomputePercent(channelIndex);
  return FeederBucketResult::Ok;
}

uint8_t FeederBucketService::enabledChannelMask() const {
  uint8_t mask = 0;
  for (uint8_t i = 0; i < kFeederMaxChannels; ++i) {
    if (snapshot_.channels[i].baseInfo.enabled) {
      mask |= static_cast<uint8_t>(1U << i);
    }
  }
  return mask;
}

FeederBucketSnapshot FeederBucketService::snapshot() const {
  return snapshot_;
}

bool FeederBucketService::validChannel(uint8_t channelIndex) const {
  return channelIndex < kFeederMaxChannels;
}

FeederBucketResult FeederBucketService::validateBaseInfo(const FeederChannelBaseInfo& info) const {
  if (info.outputPulsesPerRev <= 0 || info.gramsPerRevX100 < 0 ||
      info.capacityGramsX100 <= 0) {
    return FeederBucketResult::InvalidArgument;
  }
  return FeederBucketResult::Ok;
}

void FeederBucketService::recomputePercent(uint8_t channelIndex) {
  FeederBucketState& channel = snapshot_.channels[channelIndex];
  const int32_t capacity = channel.baseInfo.capacityGramsX100;
  if (capacity <= 0 || channel.remainGramsX100 <= 0) {
    channel.remainPercent = 0;
    return;
  }
  int32_t percent = (channel.remainGramsX100 * 100) / capacity;
  if (percent > 100) {
    percent = 100;
  }
  channel.remainPercent = static_cast<uint8_t>(percent);
}
