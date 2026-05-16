#include <cassert>

#include "FeederController.h"

int main() {
  FeederController feeder;

  FeederControllerConfig config;
  config.installedChannelMask = 0b0111;
  config.enabledChannelMask = 0b0111;
  assert(feeder.configure(config) == FeederCommandResult::Ok);
  assert(feeder.snapshot().state == FeederDeviceState::Idle);

  FeederStartResult first = feeder.startChannels(0b0001, FeederRunSource::Manual);
  assert(first.result == FeederCommandResult::Ok);
  assert(first.successMask == 0b0001);
  assert(first.busyMask == 0);
  assert(feeder.snapshot().state == FeederDeviceState::Running);
  assert(feeder.snapshot().runningChannelMask == 0b0001);

  FeederStartResult second = feeder.startChannels(0b0011, FeederRunSource::Manual);
  assert(second.result == FeederCommandResult::Partial);
  assert(second.successMask == 0b0010);
  assert(second.busyMask == 0b0001);
  assert(feeder.snapshot().runningChannelMask == 0b0011);

  assert(feeder.completeChannel(0) == FeederCommandResult::Ok);
  assert(feeder.snapshot().runningChannelMask == 0b0010);
  assert(feeder.snapshot().state == FeederDeviceState::Running);

  assert(feeder.stopChannels(0b0010) == FeederCommandResult::Ok);
  assert(feeder.snapshot().runningChannelMask == 0);
  assert(feeder.snapshot().state == FeederDeviceState::Idle);

  assert(feeder.startChannels(0b0011, FeederRunSource::Manual).result == FeederCommandResult::Ok);
  assert(feeder.stopAll() == FeederCommandResult::Ok);
  assert(feeder.snapshot().runningChannelMask == 0);
  assert(feeder.snapshot().state == FeederDeviceState::Idle);

  assert(feeder.startChannels(0b0010, FeederRunSource::Manual).result == FeederCommandResult::Ok);
  assert(feeder.setChannelFault(2) == FeederCommandResult::Ok);
  assert(feeder.snapshot().faultChannelMask == 0b0100);
  assert(feeder.startChannels(0b0100, FeederRunSource::Manual).result == FeederCommandResult::Fault);

  assert(feeder.completeChannel(1) == FeederCommandResult::Ok);
  assert(feeder.snapshot().runningChannelMask == 0);
  assert(feeder.snapshot().state == FeederDeviceState::Degraded);

  assert(feeder.clearChannelFault(2) == FeederCommandResult::Ok);
  assert(feeder.snapshot().state == FeederDeviceState::Idle);

  assert(feeder.startChannels(0b1000, FeederRunSource::Manual).result ==
         FeederCommandResult::NotConfigured);

  return 0;
}
