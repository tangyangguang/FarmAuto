#pragma once

#include <cstdint>

#include "FeederController.h"
#include "FeederTarget.h"

struct FeederRunChannel {
  bool active = false;
  FeederRunSource source = FeederRunSource::Manual;
  int32_t targetPulses = 0;
  int32_t estimatedGramsX100 = 0;
  int32_t actualPulses = 0;
};

struct FeederRunSnapshot {
  FeederRunChannel channels[kFeederMaxChannels];
};

class FeederRunTracker {
 public:
  void start(uint8_t successMask, FeederRunSource source, const FeederTargetBatch& targets);
  void stop(uint8_t channelMask);
  void stopAll();
  FeederRunSnapshot snapshot() const;

 private:
  FeederRunSnapshot snapshot_;
};
