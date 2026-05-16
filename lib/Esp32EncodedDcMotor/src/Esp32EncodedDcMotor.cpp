#include "Esp32EncodedDcMotor.h"

namespace Esp32EncodedDcMotor {

MotorResult EncodedDcMotor::begin(IMotorDriver& driver,
                                  IEncoderReader& encoder,
                                  const MotorHardwareConfig& hardware,
                                  const EncoderBackendConfig& encoderConfig) {
  driver_ = &driver;
  encoder_ = &encoder;
  hardware_ = hardware;
  encoderConfig_ = encoderConfig;
  snapshot_ = {};
  snapshot_.positionPulses = encoder_->positionPulses();
  updateTrace(0);
  return MotorResult::Ok;
}

MotorResult EncodedDcMotor::configure(const MotorKinematics& kinematics,
                                      const MotorMotionProfile& profile,
                                      const MotorProtection& protection,
                                      const MotorStopPolicy& stopPolicy) {
  if (driver_ == nullptr || encoder_ == nullptr) {
    return MotorResult::NotInitialized;
  }
  if (kinematics.outputPulsesPerRev <= 0 || profile.speedPercent == 0 ||
      profile.speedPercent > 100 || profile.minEffectiveSpeedPercent > 100) {
    return MotorResult::InvalidArgument;
  }
  kinematics_ = kinematics;
  profile_ = profile;
  protection_ = protection;
  stopPolicy_ = stopPolicy;
  configured_ = true;
  return MotorResult::Ok;
}

MotorResult EncodedDcMotor::requestMovePulses(int64_t pulses) {
  if (pulses == 0) {
    return MotorResult::TargetTooSmall;
  }
  if (encoder_ == nullptr) {
    return MotorResult::NotInitialized;
  }
  return startMoveTo(encoder_->positionPulses() + pulses);
}

MotorResult EncodedDcMotor::requestMoveToPosition(int64_t positionPulses) {
  return startMoveTo(positionPulses);
}

MotorResult EncodedDcMotor::requestStop() {
  if (driver_ == nullptr) {
    return MotorResult::NotInitialized;
  }
  snapshot_.activeCommand = MotorCommand::Stop;
  snapshot_.state = profile_.softStopMs > 0 ? MotorState::SoftStopping : MotorState::Idle;
  snapshot_.targetSpeedPercent = 0;
  snapshot_.currentSpeedPercent = 0;
  snapshot_.driverOutputPercent = 0;
  snapshot_.direction = MotorDirection::Stopped;
  snapshot_.lastCommandResult = driver_->stop(stopPolicy_.emergencyOutputMode);
  return snapshot_.lastCommandResult;
}

MotorResult EncodedDcMotor::requestEmergencyStop(FaultReason reason) {
  if (driver_ == nullptr) {
    return MotorResult::NotInitialized;
  }
  snapshot_.activeCommand = MotorCommand::EmergencyStop;
  snapshot_.state = MotorState::Fault;
  snapshot_.faultReason = reason;
  snapshot_.targetSpeedPercent = 0;
  snapshot_.currentSpeedPercent = 0;
  snapshot_.driverOutputPercent = 0;
  snapshot_.direction = MotorDirection::Stopped;
  snapshot_.lastCommandResult = driver_->stop(stopPolicy_.emergencyOutputMode);
  return snapshot_.lastCommandResult;
}

void EncodedDcMotor::setCurrentPositionPulses(int64_t positionPulses) {
  snapshot_.positionPulses = positionPulses;
  snapshot_.segmentStartPulses = positionPulses;
}

void EncodedDcMotor::update(uint32_t nowMs) {
  if (driver_ == nullptr || encoder_ == nullptr || !configured_) {
    return;
  }

  const int64_t previousPosition = snapshot_.positionPulses;
  snapshot_.positionPulses = encoder_->positionPulses();
  snapshot_.encoderDeltaSinceLastCheck = snapshot_.positionPulses - previousPosition;
  snapshot_.remainingPulses = snapshot_.targetPulses - snapshot_.positionPulses;
  snapshot_.elapsedMs = nowMs - commandStartMs_;
  snapshot_.lastUpdateMs = nowMs;

  const bool reachedTarget =
      (snapshot_.direction == MotorDirection::Forward &&
       snapshot_.positionPulses >= snapshot_.targetPulses) ||
      (snapshot_.direction == MotorDirection::Reverse &&
       snapshot_.positionPulses <= snapshot_.targetPulses);
  if ((snapshot_.state == MotorState::SoftStarting || snapshot_.state == MotorState::Running ||
       snapshot_.state == MotorState::SoftStopping) &&
      reachedTarget) {
    snapshot_.lastCommandResult = driver_->stop(stopPolicy_.emergencyOutputMode);
    snapshot_.state = MotorState::Idle;
    snapshot_.activeCommand = MotorCommand::None;
    snapshot_.direction = MotorDirection::Stopped;
    snapshot_.targetSpeedPercent = 0;
    snapshot_.currentSpeedPercent = 0;
    snapshot_.driverOutputPercent = 0;
    snapshot_.remainingPulses = 0;
    updateTrace(nowMs);
    return;
  }

  if (snapshot_.state == MotorState::SoftStarting || snapshot_.state == MotorState::Running) {
    const uint8_t output = outputForElapsed(snapshot_.elapsedMs);
    const int8_t direction = static_cast<int8_t>(snapshot_.direction);
    const MotorResult result = driver_->setOutput(direction, output);
    snapshot_.lastCommandResult = result;
    if (result != MotorResult::Ok) {
      requestEmergencyStop(FaultReason::DriverRejected);
      return;
    }
    snapshot_.driverOutputPercent = output;
    snapshot_.currentSpeedPercent = output;
    if (profile_.softStartMs == 0 || snapshot_.elapsedMs >= profile_.softStartMs) {
      snapshot_.state = MotorState::Running;
      snapshot_.currentSpeedPercent = profile_.speedPercent;
      snapshot_.driverOutputPercent = profile_.speedPercent;
    }
  }

  updateTrace(nowMs);
}

MotorSnapshot EncodedDcMotor::snapshot() const {
  return snapshot_;
}

MotorTracePoint EncodedDcMotor::latestTracePoint() const {
  return trace_;
}

MotorResult EncodedDcMotor::startMoveTo(int64_t targetPulses) {
  if (driver_ == nullptr || encoder_ == nullptr) {
    return MotorResult::NotInitialized;
  }
  if (!configured_) {
    return MotorResult::ConfigMissing;
  }
  if (snapshot_.state != MotorState::Idle) {
    return MotorResult::Busy;
  }

  const int64_t current = encoder_->positionPulses();
  if (targetPulses == current) {
    return MotorResult::AlreadyAtTarget;
  }

  snapshot_.activeCommand = MotorCommand::MoveToPosition;
  snapshot_.state = profile_.softStartMs > 0 ? MotorState::SoftStarting : MotorState::Running;
  snapshot_.direction = targetPulses > current ? MotorDirection::Forward : MotorDirection::Reverse;
  snapshot_.segmentStartPulses = current;
  snapshot_.positionPulses = current;
  snapshot_.targetPulses = targetPulses;
  snapshot_.remainingPulses = targetPulses - current;
  snapshot_.targetSpeedPercent = profile_.speedPercent;
  snapshot_.faultReason = FaultReason::None;
  snapshot_.lastCommandResult = MotorResult::Ok;
  commandStartMs_ = 0;
  return MotorResult::Ok;
}

uint8_t EncodedDcMotor::outputForElapsed(uint32_t elapsedMs) const {
  if (profile_.softStartMs == 0 || elapsedMs >= profile_.softStartMs) {
    return profile_.speedPercent;
  }
  const uint32_t scaled = static_cast<uint32_t>(profile_.speedPercent) * elapsedMs / profile_.softStartMs;
  const uint8_t output = static_cast<uint8_t>(scaled);
  if (output < profile_.minEffectiveSpeedPercent) {
    return profile_.minEffectiveSpeedPercent;
  }
  return output;
}

void EncodedDcMotor::updateTrace(uint32_t nowMs) {
  trace_.timestampMs = nowMs;
  trace_.state = snapshot_.state;
  trace_.activeCommand = snapshot_.activeCommand;
  trace_.direction = snapshot_.direction;
  trace_.positionPulses = snapshot_.positionPulses;
  trace_.targetPulses = snapshot_.targetPulses;
  trace_.remainingPulses = snapshot_.remainingPulses;
  trace_.pulsesPerSecond = snapshot_.pulsesPerSecond;
  trace_.rpm = snapshot_.rpm;
  trace_.targetSpeedPercent = snapshot_.targetSpeedPercent;
  trace_.driverOutputPercent = snapshot_.driverOutputPercent;
  trace_.encoderDelta = snapshot_.encoderDeltaSinceLastCheck;
  trace_.faultReason = snapshot_.faultReason;
}

}  // namespace Esp32EncodedDcMotor
