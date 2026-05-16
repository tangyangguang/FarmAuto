#include "FeederController.h"

FeederCommandResult FeederController::configure(const FeederControllerConfig& config) {
  const uint8_t mask = validMask();
  snapshot_ = FeederSnapshot{};
  snapshot_.installedChannelMask = config.installedChannelMask & mask;
  snapshot_.enabledChannelMask = config.enabledChannelMask & snapshot_.installedChannelMask;

  for (uint8_t i = 0; i < kFeederMaxChannels; ++i) {
    snapshot_.channels[i] = channelInMask(snapshot_.enabledChannelMask, i)
                                ? FeederChannelState::Idle
                                : FeederChannelState::Disabled;
  }
  recomputeState();
  return snapshot_.enabledChannelMask == 0 ? FeederCommandResult::NotConfigured
                                           : FeederCommandResult::Ok;
}

FeederStartResult FeederController::startChannels(uint8_t requestedMask, FeederRunSource source) {
  (void)source;

  FeederStartResult result;
  requestedMask &= validMask();
  if (requestedMask == 0) {
    result.result = FeederCommandResult::InvalidArgument;
    return result;
  }

  const uint8_t unavailableMask = requestedMask & ~snapshot_.enabledChannelMask;
  result.skippedMask |= unavailableMask;

  for (uint8_t i = 0; i < kFeederMaxChannels; ++i) {
    const uint8_t bit = static_cast<uint8_t>(1U << i);
    if ((requestedMask & bit) == 0 || (unavailableMask & bit) != 0) {
      continue;
    }

    if (snapshot_.channels[i] == FeederChannelState::Running) {
      result.busyMask |= bit;
    } else if (snapshot_.channels[i] == FeederChannelState::Fault) {
      result.faultMask |= bit;
    } else {
      snapshot_.channels[i] = FeederChannelState::Running;
      result.successMask |= bit;
    }
  }

  recomputeState();
  if (result.successMask != 0 && (result.busyMask != 0 || result.faultMask != 0 || result.skippedMask != 0)) {
    result.result = FeederCommandResult::Partial;
  } else if (result.successMask != 0) {
    result.result = FeederCommandResult::Ok;
  } else if (result.faultMask != 0) {
    result.result = FeederCommandResult::Fault;
  } else if (result.busyMask != 0) {
    result.result = FeederCommandResult::Busy;
  } else {
    result.result = FeederCommandResult::NotConfigured;
  }
  return result;
}

FeederCommandResult FeederController::completeChannel(uint8_t channelIndex) {
  if (!validChannel(channelIndex)) {
    return FeederCommandResult::InvalidArgument;
  }
  if (snapshot_.channels[channelIndex] != FeederChannelState::Running) {
    return FeederCommandResult::Busy;
  }
  snapshot_.channels[channelIndex] = FeederChannelState::Completed;
  recomputeState();
  return FeederCommandResult::Ok;
}

FeederCommandResult FeederController::setChannelFault(uint8_t channelIndex) {
  if (!validChannel(channelIndex)) {
    return FeederCommandResult::InvalidArgument;
  }
  if (!channelInMask(snapshot_.enabledChannelMask, channelIndex)) {
    return FeederCommandResult::NotConfigured;
  }
  snapshot_.channels[channelIndex] = FeederChannelState::Fault;
  recomputeState();
  return FeederCommandResult::Ok;
}

FeederCommandResult FeederController::clearChannelFault(uint8_t channelIndex) {
  if (!validChannel(channelIndex)) {
    return FeederCommandResult::InvalidArgument;
  }
  if (snapshot_.channels[channelIndex] != FeederChannelState::Fault) {
    return FeederCommandResult::InvalidArgument;
  }
  snapshot_.channels[channelIndex] = FeederChannelState::Idle;
  recomputeState();
  return FeederCommandResult::Ok;
}

FeederSnapshot FeederController::snapshot() const {
  return snapshot_;
}

uint8_t FeederController::validMask() {
  return static_cast<uint8_t>((1U << kFeederMaxChannels) - 1U);
}

bool FeederController::channelInMask(uint8_t mask, uint8_t channelIndex) {
  return channelIndex < kFeederMaxChannels && (mask & (1U << channelIndex)) != 0;
}

bool FeederController::validChannel(uint8_t channelIndex) const {
  return channelIndex < kFeederMaxChannels && channelInMask(snapshot_.installedChannelMask, channelIndex);
}

void FeederController::recomputeState() {
  snapshot_.runningChannelMask = 0;
  snapshot_.faultChannelMask = 0;
  snapshot_.runningCount = 0;

  for (uint8_t i = 0; i < kFeederMaxChannels; ++i) {
    const uint8_t bit = static_cast<uint8_t>(1U << i);
    if (snapshot_.channels[i] == FeederChannelState::Running) {
      snapshot_.runningChannelMask |= bit;
      ++snapshot_.runningCount;
    } else if (snapshot_.channels[i] == FeederChannelState::Fault) {
      snapshot_.faultChannelMask |= bit;
    }
  }

  if (snapshot_.runningChannelMask != 0) {
    snapshot_.state = FeederDeviceState::Running;
  } else if (snapshot_.enabledChannelMask == 0 ||
             (snapshot_.faultChannelMask & snapshot_.enabledChannelMask) == snapshot_.enabledChannelMask) {
    snapshot_.state = FeederDeviceState::Fault;
  } else if ((snapshot_.faultChannelMask & snapshot_.enabledChannelMask) != 0) {
    snapshot_.state = FeederDeviceState::Degraded;
  } else {
    snapshot_.state = FeederDeviceState::Idle;
  }
}
