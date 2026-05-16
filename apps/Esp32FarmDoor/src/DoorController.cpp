#include "DoorController.h"

DoorCommandResult DoorController::configure(const DoorControllerConfig& config) {
  if (config.openTargetPulses <= config.closedPositionPulses || config.maxRunPulses <= 0 ||
      config.maxRunMs == 0) {
    snapshot_.state = DoorState::Fault;
    snapshot_.faultReason = DoorFaultReason::InvalidConfig;
    return DoorCommandResult::InvalidArgument;
  }

  config_ = config;
  snapshot_.openTargetPulses = config.openTargetPulses;
  snapshot_.closedPositionPulses = config.closedPositionPulses;
  if (snapshot_.positionTrustLevel == PositionTrustLevel::Untrusted) {
    snapshot_.state = DoorState::PositionUnknown;
  }
  return DoorCommandResult::Ok;
}

DoorCommandResult DoorController::markPositionClosed() {
  if (!configured()) {
    return DoorCommandResult::InvalidArgument;
  }
  if (isRunning()) {
    return DoorCommandResult::Busy;
  }
  snapshot_.positionPulses = config_.closedPositionPulses;
  snapshot_.targetPulses = config_.closedPositionPulses;
  snapshot_.state = DoorState::IdleClosed;
  snapshot_.activeCommand = DoorCommand::None;
  snapshot_.positionTrustLevel = PositionTrustLevel::Trusted;
  snapshot_.lastStopReason = DoorStopReason::TargetReached;
  snapshot_.faultReason = DoorFaultReason::None;
  return DoorCommandResult::Ok;
}

DoorCommandResult DoorController::markPositionOpen() {
  if (!configured()) {
    return DoorCommandResult::InvalidArgument;
  }
  if (isRunning()) {
    return DoorCommandResult::Busy;
  }
  snapshot_.positionPulses = config_.openTargetPulses;
  snapshot_.targetPulses = config_.openTargetPulses;
  snapshot_.state = DoorState::IdleOpen;
  snapshot_.activeCommand = DoorCommand::None;
  snapshot_.positionTrustLevel = PositionTrustLevel::Trusted;
  snapshot_.lastStopReason = DoorStopReason::TargetReached;
  snapshot_.faultReason = DoorFaultReason::None;
  return DoorCommandResult::Ok;
}

DoorCommandResult DoorController::setTrustedPosition(int64_t positionPulses,
                                                     PositionTrustLevel trustLevel) {
  if (!configured()) {
    return DoorCommandResult::InvalidArgument;
  }
  if (isRunning()) {
    return DoorCommandResult::Busy;
  }
  snapshot_.positionPulses = positionPulses;
  snapshot_.targetPulses = positionPulses;
  snapshot_.positionTrustLevel = trustLevel;
  snapshot_.activeCommand = DoorCommand::None;
  snapshot_.faultReason = DoorFaultReason::None;

  if (trustLevel == PositionTrustLevel::Untrusted) {
    snapshot_.state = DoorState::PositionUnknown;
  } else if (positionPulses <= config_.closedPositionPulses) {
    snapshot_.state = DoorState::IdleClosed;
  } else if (positionPulses >= config_.openTargetPulses) {
    snapshot_.state = DoorState::IdleOpen;
  } else {
    snapshot_.state = DoorState::IdlePartial;
  }
  return DoorCommandResult::Ok;
}

DoorCommandResult DoorController::requestOpen() {
  return requestMove(DoorCommand::Open, DoorState::Opening, config_.openTargetPulses);
}

DoorCommandResult DoorController::requestClose() {
  return requestMove(DoorCommand::Close, DoorState::Closing, config_.closedPositionPulses);
}

DoorCommandResult DoorController::requestStop(int64_t stoppedPositionPulses) {
  if (!isRunning()) {
    return DoorCommandResult::InvalidArgument;
  }
  snapshot_.positionPulses = stoppedPositionPulses;
  snapshot_.targetPulses = stoppedPositionPulses;
  snapshot_.state = DoorState::IdlePartial;
  snapshot_.activeCommand = DoorCommand::None;
  snapshot_.positionTrustLevel = PositionTrustLevel::Trusted;
  snapshot_.lastStopReason = DoorStopReason::UserStop;
  return DoorCommandResult::Ok;
}

DoorCommandResult DoorController::onMotionTargetReached(int64_t finalPositionPulses) {
  if (!isRunning()) {
    return DoorCommandResult::InvalidArgument;
  }
  const DoorCommand completedCommand = snapshot_.activeCommand;
  snapshot_.positionPulses = finalPositionPulses;
  snapshot_.targetPulses = finalPositionPulses;
  snapshot_.activeCommand = DoorCommand::None;
  snapshot_.positionTrustLevel = PositionTrustLevel::Trusted;
  snapshot_.lastStopReason = DoorStopReason::TargetReached;
  snapshot_.faultReason = DoorFaultReason::None;
  snapshot_.state = completedCommand == DoorCommand::Open ? DoorState::IdleOpen : DoorState::IdleClosed;
  return DoorCommandResult::Ok;
}

DoorCommandResult DoorController::enterFault(DoorFaultReason reason) {
  snapshot_.state = DoorState::Fault;
  snapshot_.activeCommand = DoorCommand::None;
  snapshot_.faultReason = reason;
  snapshot_.lastStopReason = DoorStopReason::FaultStop;
  return DoorCommandResult::Ok;
}

DoorSnapshot DoorController::snapshot() const {
  return snapshot_;
}

bool DoorController::configured() const {
  return snapshot_.openTargetPulses > snapshot_.closedPositionPulses && config_.maxRunPulses > 0 &&
         config_.maxRunMs > 0;
}

bool DoorController::isRunning() const {
  return snapshot_.state == DoorState::Opening || snapshot_.state == DoorState::Closing;
}

DoorCommandResult DoorController::requestMove(DoorCommand command,
                                              DoorState runningState,
                                              int64_t targetPulses) {
  if (!configured()) {
    return DoorCommandResult::InvalidArgument;
  }
  if (snapshot_.state == DoorState::Fault) {
    return DoorCommandResult::FaultActive;
  }
  if (isRunning()) {
    return DoorCommandResult::Busy;
  }
  if (snapshot_.positionTrustLevel == PositionTrustLevel::Untrusted) {
    return DoorCommandResult::PositionUntrusted;
  }
  snapshot_.activeCommand = command;
  snapshot_.state = runningState;
  snapshot_.targetPulses = targetPulses;
  snapshot_.lastStopReason = DoorStopReason::None;
  return DoorCommandResult::Ok;
}
