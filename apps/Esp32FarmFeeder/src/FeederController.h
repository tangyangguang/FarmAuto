#pragma once

#include <cstdint>

static constexpr uint8_t kFeederMaxChannels = 4;

enum class FeederDeviceState : uint8_t {
  Idle,
  Running,
  Degraded,
  Fault
};

enum class FeederChannelState : uint8_t {
  Disabled,
  Idle,
  Running,
  Completed,
  Fault
};

enum class FeederRunSource : uint8_t {
  Manual,
  Schedule
};

enum class FeederCommandResult : uint8_t {
  Ok,
  Partial,
  Busy,
  NotConfigured,
  Fault,
  InvalidArgument
};

struct FeederControllerConfig {
  uint8_t installedChannelMask = 0;
  uint8_t enabledChannelMask = 0;
};

struct FeederStartResult {
  FeederCommandResult result = FeederCommandResult::InvalidArgument;
  uint8_t successMask = 0;
  uint8_t busyMask = 0;
  uint8_t faultMask = 0;
  uint8_t skippedMask = 0;
};

struct FeederSnapshot {
  FeederDeviceState state = FeederDeviceState::Idle;
  uint8_t installedChannelMask = 0;
  uint8_t enabledChannelMask = 0;
  uint8_t runningChannelMask = 0;
  uint8_t faultChannelMask = 0;
  uint8_t runningCount = 0;
  FeederChannelState channels[kFeederMaxChannels] = {
      FeederChannelState::Disabled,
      FeederChannelState::Disabled,
      FeederChannelState::Disabled,
      FeederChannelState::Disabled,
  };
};

class FeederController {
 public:
  FeederCommandResult configure(const FeederControllerConfig& config);
  FeederStartResult startChannels(uint8_t requestedMask, FeederRunSource source);
  FeederCommandResult stopChannels(uint8_t requestedMask);
  FeederCommandResult stopAll();
  FeederCommandResult completeChannel(uint8_t channelIndex);
  FeederCommandResult setChannelFault(uint8_t channelIndex);
  FeederCommandResult clearChannelFault(uint8_t channelIndex);
  FeederSnapshot snapshot() const;

 private:
  static uint8_t validMask();
  static bool channelInMask(uint8_t mask, uint8_t channelIndex);
  bool validChannel(uint8_t channelIndex) const;
  void recomputeState();

  FeederSnapshot snapshot_;
};
