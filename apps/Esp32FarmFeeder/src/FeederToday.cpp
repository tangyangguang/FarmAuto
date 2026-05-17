#include "FeederToday.h"

void FeederTodayService::beginDay(uint32_t serviceDate) {
  snapshot_ = FeederTodaySnapshot();
  snapshot_.serviceDate = serviceDate;
}

FeederTodayResult FeederTodayService::addChannelFeed(uint8_t channelIndex,
                                                     int32_t pulses,
                                                     int32_t gramsX100) {
  if (!validChannel(channelIndex) || pulses < 0 || gramsX100 < 0) {
    return FeederTodayResult::InvalidArgument;
  }
  snapshot_.channels[channelIndex].pulses += pulses;
  snapshot_.channels[channelIndex].gramsX100 += gramsX100;
  return FeederTodayResult::Ok;
}

FeederTodayResult FeederTodayService::restore(const FeederTodaySnapshot& snapshot) {
  if (!validSnapshot(snapshot)) {
    return FeederTodayResult::InvalidArgument;
  }
  snapshot_ = snapshot;
  return FeederTodayResult::Ok;
}

FeederTodaySnapshot FeederTodayService::snapshot() const {
  return snapshot_;
}

bool FeederTodayService::validChannel(uint8_t channelIndex) {
  return channelIndex < kFeederConfiguredChannels;
}

bool FeederTodayService::validSnapshot(const FeederTodaySnapshot& snapshot) {
  for (uint8_t i = 0; i < kFeederMaxChannels; ++i) {
    if (snapshot.channels[i].pulses < 0 || snapshot.channels[i].gramsX100 < 0) {
      return false;
    }
  }
  return true;
}
