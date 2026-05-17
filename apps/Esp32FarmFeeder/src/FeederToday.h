#pragma once

#include <cstdint>

#include "FeederController.h"

enum class FeederTodayResult : uint8_t {
  Ok,
  InvalidArgument
};

struct FeederTodayChannel {
  int32_t pulses = 0;
  int32_t gramsX100 = 0;
};

struct FeederTodaySnapshot {
  uint32_t serviceDate = 0;
  FeederTodayChannel channels[kFeederMaxChannels];
};

class FeederTodayService {
 public:
  void beginDay(uint32_t serviceDate);
  FeederTodayResult addChannelFeed(uint8_t channelIndex, int32_t pulses, int32_t gramsX100);
  FeederTodayResult restore(const FeederTodaySnapshot& snapshot);
  FeederTodaySnapshot snapshot() const;

 private:
  static bool validChannel(uint8_t channelIndex);
  static bool validSnapshot(const FeederTodaySnapshot& snapshot);

  FeederTodaySnapshot snapshot_;
};
