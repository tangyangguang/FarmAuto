#include "FeederRunTracker.h"

void FeederRunTracker::start(uint8_t successMask, FeederRunSource source,
                             const FeederTargetBatch& targets) {
  for (uint8_t i = 0; i < kFeederMaxChannels; ++i) {
    const uint8_t bit = static_cast<uint8_t>(1U << i);
    if ((successMask & bit) == 0) {
      continue;
    }
    snapshot_.channels[i].active = true;
    snapshot_.channels[i].source = source;
    snapshot_.channels[i].targetPulses = targets.channels[i].targetPulses;
    snapshot_.channels[i].estimatedGramsX100 = targets.channels[i].estimatedGramsX100;
    snapshot_.channels[i].actualPulses = 0;
  }
}

void FeederRunTracker::stop(uint8_t channelMask) {
  for (uint8_t i = 0; i < kFeederMaxChannels; ++i) {
    const uint8_t bit = static_cast<uint8_t>(1U << i);
    if ((channelMask & bit) != 0) {
      snapshot_.channels[i] = FeederRunChannel{};
    }
  }
}

void FeederRunTracker::stopAll() {
  snapshot_ = FeederRunSnapshot{};
}

FeederRunSnapshot FeederRunTracker::snapshot() const {
  return snapshot_;
}
