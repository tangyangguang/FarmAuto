#include "DoorRecoveryApply.h"

DoorCommandResult applyDoorRecoveryState(DoorController& door, const DoorRecoveryState& state) {
  DoorControllerConfig config;
  config.closedPositionPulses = state.closedPositionPulses;
  config.openTargetPulses = state.openTargetPulses;
  config.maxRunPulses = state.maxRunPulses;
  config.maxRunMs = state.maxRunMs;

  const DoorCommandResult configureResult = door.configure(config);
  if (configureResult != DoorCommandResult::Ok) {
    return configureResult;
  }
  return door.setTrustedPosition(state.positionPulses, state.positionTrustLevel);
}

DoorRecoveryState doorRecoveryStateFromSnapshot(const DoorSnapshot& snapshot,
                                                const DoorControllerConfig& config,
                                                uint32_t unixTime,
                                                uint32_t uptimeSec,
                                                uint32_t bootId) {
  DoorRecoveryState state;
  state.positionPulses = snapshot.positionPulses;
  state.openTargetPulses = config.openTargetPulses;
  state.closedPositionPulses = config.closedPositionPulses;
  state.maxRunPulses = config.maxRunPulses;
  state.maxRunMs = config.maxRunMs;
  state.positionTrustLevel = snapshot.positionTrustLevel;
  state.lastCommand = snapshot.activeCommand;
  state.lastStopReason = snapshot.lastStopReason;
  state.unixTime = unixTime;
  state.uptimeSec = uptimeSec;
  state.bootId = bootId;
  return state;
}
