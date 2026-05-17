#pragma once

#include <cstdint>

#include "FeederController.h"

static constexpr uint8_t kFeederChannelNameMaxBytes = 16;

enum class FeederBucketResult : uint8_t {
  Ok,
  InvalidArgument,
  Underflow
};

struct FeederChannelBaseInfo {
  char name[kFeederChannelNameMaxBytes + 1] = {};
  bool enabled = false;
  int32_t outputPulsesPerRev = 0;
  int32_t gramsPerRevX100 = 0;
  int32_t capacityGramsX100 = 0;
};

struct FeederBucketState {
  FeederChannelBaseInfo baseInfo;
  int32_t remainGramsX100 = 0;
  uint8_t remainPercent = 0;
  uint32_t lastRefillUnixTime = 0;
  bool underflow = false;
};

struct FeederBucketSnapshot {
  FeederBucketState channels[kFeederMaxChannels];
};

class FeederBucketService {
 public:
  FeederBucketResult updateBaseInfo(uint8_t channelIndex, const FeederChannelBaseInfo& info);
  FeederBucketResult setRemaining(uint8_t channelIndex, int32_t remainGramsX100, uint32_t unixTime);
  FeederBucketResult addFeed(uint8_t channelIndex, int32_t addedGramsX100, uint32_t unixTime);
  FeederBucketResult markFull(uint8_t channelIndex, uint32_t unixTime);
  FeederBucketResult consume(uint8_t channelIndex, int32_t usedGramsX100);
  uint8_t enabledChannelMask() const;
  FeederBucketSnapshot snapshot() const;
  FeederBucketResult restore(const FeederBucketSnapshot& snapshot);

 private:
  bool validChannel(uint8_t channelIndex) const;
  FeederBucketResult validateBaseInfo(const FeederChannelBaseInfo& info) const;
  void recomputePercent(uint8_t channelIndex);

  FeederBucketSnapshot snapshot_;
};
