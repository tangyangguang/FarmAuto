#include <cassert>
#include <climits>

#include "FeederToday.h"

int main() {
  FeederTodayService today;
  today.beginDay(20260517);

  assert(today.addChannelFeed(0, 4320, 7000) == FeederTodayResult::Ok);
  assert(today.addChannelFeed(0, 2160, 3500) == FeederTodayResult::Ok);
  assert(today.addChannelFeed(1, 4000, 6500) == FeederTodayResult::Ok);

  FeederTodaySnapshot snapshot = today.snapshot();
  assert(snapshot.serviceDate == 20260517);
  assert(snapshot.channels[0].pulses == 6480);
  assert(snapshot.channels[0].gramsX100 == 10500);
  assert(snapshot.channels[1].pulses == 4000);
  assert(snapshot.channels[1].gramsX100 == 6500);

  assert(today.addChannelFeed(4, 1, 1) == FeederTodayResult::InvalidArgument);
  assert(today.addChannelFeed(0, -1, 1) == FeederTodayResult::InvalidArgument);
  assert(today.addChannelFeed(0, 1, -1) == FeederTodayResult::InvalidArgument);

  FeederTodaySnapshot restored;
  restored.serviceDate = 20260518;
  restored.channels[2].pulses = 123;
  restored.channels[2].gramsX100 = 456;
  assert(today.restore(restored) == FeederTodayResult::Ok);
  snapshot = today.snapshot();
  assert(snapshot.serviceDate == 20260518);
  assert(snapshot.channels[2].pulses == 123);
  assert(snapshot.channels[2].gramsX100 == 456);

  restored.channels[0].pulses = -1;
  assert(today.restore(restored) == FeederTodayResult::InvalidArgument);

  FeederTodaySnapshot nearLimit;
  nearLimit.serviceDate = 20260519;
  nearLimit.channels[0].pulses = INT_MAX - 5;
  nearLimit.channels[0].gramsX100 = INT_MAX - 5;
  assert(today.restore(nearLimit) == FeederTodayResult::Ok);
  assert(today.addChannelFeed(0, 5, 5) == FeederTodayResult::Ok);
  assert(today.snapshot().channels[0].pulses == INT_MAX);
  assert(today.addChannelFeed(0, 1, 0) == FeederTodayResult::InvalidArgument);
  assert(today.addChannelFeed(0, 0, 1) == FeederTodayResult::InvalidArgument);

  return 0;
}
