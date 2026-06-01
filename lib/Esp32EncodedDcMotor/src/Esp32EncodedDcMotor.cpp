#include "Esp32EncodedDcMotor.h"

#if defined(ARDUINO)
#include <Arduino.h>
#include <driver/pcnt.h>
#endif

namespace Esp32EncodedDcMotor {

uint32_t pwmMaxDuty(uint8_t resolutionBits) {
  if (resolutionBits == 0 || resolutionBits >= 31) {
    return 0;
  }
  return (1UL << resolutionBits) - 1UL;
}

uint32_t percentToDuty(uint8_t percent, uint8_t resolutionBits, bool polarity) {
  const uint32_t maxDuty = pwmMaxDuty(resolutionBits);
  if (maxDuty == 0) {
    return 0;
  }
  if (percent > 100) {
    percent = 100;
  }
  const uint32_t duty = (maxDuty * static_cast<uint32_t>(percent) + 50UL) / 100UL;
  return polarity ? duty : maxDuty - duty;
}

DualPwmOutput at8236OutputFor(int8_t direction,
                              uint8_t percent,
                              uint8_t resolutionBits,
                              bool polarity) {
  DualPwmOutput output;
  if (direction == 0 || percent == 0) {
    return output;
  }
  const uint32_t duty = percentToDuty(percent, resolutionBits, polarity);
  if (direction > 0) {
    output.dutyA = duty;
  } else {
    output.dutyB = duty;
  }
  return output;
}

#if defined(ARDUINO)
namespace {

bool validLedcChannel(int8_t channel) {
  return channel >= 0 && channel < 16;
}

MotorResult setupLedc(uint8_t pin, int8_t channel, const MotorHardwareConfig& config) {
  if (!validLedcChannel(channel) || config.pwmFrequencyHz == 0 ||
      config.pwmResolutionBits == 0 || config.pwmResolutionBits > 14) {
    return MotorResult::InvalidArgument;
  }
  ledcSetup(static_cast<uint8_t>(channel), config.pwmFrequencyHz, config.pwmResolutionBits);
  ledcAttachPin(pin, static_cast<uint8_t>(channel));
  ledcWrite(static_cast<uint8_t>(channel), config.pwmPolarity ? 0 : pwmMaxDuty(config.pwmResolutionBits));
  return MotorResult::Ok;
}

}  // namespace

MotorResult At8236HBridgeDriver::begin(uint8_t pinA,
                                       uint8_t pinB,
                                       const MotorHardwareConfig& config) {
  if (config.driverType != DriverType::At8236HBridge ||
      config.ledcChannelA == config.ledcChannelB) {
    return MotorResult::InvalidArgument;
  }
  MotorResult result = setupLedc(pinA, config.ledcChannelA, config);
  if (result != MotorResult::Ok) {
    return result;
  }
  result = setupLedc(pinB, config.ledcChannelB, config);
  if (result != MotorResult::Ok) {
    return result;
  }
  pinA_ = pinA;
  pinB_ = pinB;
  config_ = config;
  initialized_ = true;
  return stop(EmergencyOutputMode::Coast);
}

MotorResult At8236HBridgeDriver::setOutput(int8_t direction, uint8_t percent) {
  if (!initialized_) {
    return MotorResult::NotInitialized;
  }
  const DualPwmOutput output =
      at8236OutputFor(direction, percent, config_.pwmResolutionBits, config_.pwmPolarity);
  ledcWrite(static_cast<uint8_t>(config_.ledcChannelA), output.dutyA);
  ledcWrite(static_cast<uint8_t>(config_.ledcChannelB), output.dutyB);
  return MotorResult::Ok;
}

MotorResult At8236HBridgeDriver::stop(EmergencyOutputMode mode) {
  if (!initialized_) {
    return MotorResult::NotInitialized;
  }
  const uint32_t brakeDuty = pwmMaxDuty(config_.pwmResolutionBits);
  const uint32_t idleDuty = config_.pwmPolarity ? 0 : brakeDuty;
  const uint32_t duty = mode == EmergencyOutputMode::Brake ? brakeDuty : idleDuty;
  ledcWrite(static_cast<uint8_t>(config_.ledcChannelA), duty);
  ledcWrite(static_cast<uint8_t>(config_.ledcChannelB), duty);
  return MotorResult::Ok;
}

MotorResult SinglePwmMotorDriver::begin(uint8_t pwmPin, const MotorHardwareConfig& config) {
  if (config.driverType != DriverType::SinglePwm) {
    return MotorResult::InvalidArgument;
  }
  const MotorResult result = setupLedc(pwmPin, config.ledcChannelA, config);
  if (result != MotorResult::Ok) {
    return result;
  }
  pwmPin_ = pwmPin;
  config_ = config;
  initialized_ = true;
  return stop(EmergencyOutputMode::Coast);
}

MotorResult SinglePwmMotorDriver::setOutput(int8_t direction, uint8_t percent) {
  if (!initialized_) {
    return MotorResult::NotInitialized;
  }
  if (direction == 0) {
    return stop(EmergencyOutputMode::Coast);
  }
  ledcWrite(static_cast<uint8_t>(config_.ledcChannelA),
            percentToDuty(percent, config_.pwmResolutionBits, config_.pwmPolarity));
  return MotorResult::Ok;
}

MotorResult SinglePwmMotorDriver::stop(EmergencyOutputMode) {
  if (!initialized_) {
    return MotorResult::NotInitialized;
  }
  ledcWrite(static_cast<uint8_t>(config_.ledcChannelA),
            config_.pwmPolarity ? 0 : pwmMaxDuty(config_.pwmResolutionBits));
  return MotorResult::Ok;
}

MotorResult PcntEncoderReader::begin(const EncoderBackendConfig& config) {
  if (config.backendType != EncoderBackendType::Pcnt || config.countMode != CountMode::X1 ||
      config.pinA < 0 || config.pinB < 0 || config.pcntUnit < 0 || config.pcntUnit >= PCNT_UNIT_MAX) {
    return MotorResult::InvalidArgument;
  }
  pcnt_config_t pcntConfig = {};
  pcntConfig.pulse_gpio_num = config.pinA;
  pcntConfig.ctrl_gpio_num = config.pinB;
  pcntConfig.channel = PCNT_CHANNEL_0;
  pcntConfig.unit = static_cast<pcnt_unit_t>(config.pcntUnit);
  pcntConfig.pos_mode = PCNT_COUNT_INC;
  pcntConfig.neg_mode = PCNT_COUNT_DIS;
  pcntConfig.lctrl_mode = PCNT_MODE_REVERSE;
  pcntConfig.hctrl_mode = PCNT_MODE_KEEP;
  pcntConfig.counter_h_lim = 30000;
  pcntConfig.counter_l_lim = -30000;
  if (pcnt_unit_config(&pcntConfig) != ESP_OK) {
    return MotorResult::DriverRejected;
  }
  if (config.glitchFilterNs > 0) {
    const uint16_t filterValue = static_cast<uint16_t>(config.glitchFilterNs / 1000U);
    pcnt_set_filter_value(static_cast<pcnt_unit_t>(config.pcntUnit), filterValue);
    pcnt_filter_enable(static_cast<pcnt_unit_t>(config.pcntUnit));
  } else {
    pcnt_filter_disable(static_cast<pcnt_unit_t>(config.pcntUnit));
  }
  pcnt_counter_pause(static_cast<pcnt_unit_t>(config.pcntUnit));
  pcnt_counter_clear(static_cast<pcnt_unit_t>(config.pcntUnit));
  pcnt_counter_resume(static_cast<pcnt_unit_t>(config.pcntUnit));
  config_ = config;
  positionPulses_ = 0;
  initialized_ = true;
  return MotorResult::Ok;
}

int64_t PcntEncoderReader::positionPulses() const {
  if (!initialized_) {
    return positionPulses_;
  }
  int16_t count = 0;
  if (pcnt_get_counter_value(static_cast<pcnt_unit_t>(config_.pcntUnit), &count) == ESP_OK &&
      count != 0) {
    positionPulses_ += count;
    pcnt_counter_clear(static_cast<pcnt_unit_t>(config_.pcntUnit));
  }
  return positionPulses_;
}

void PcntEncoderReader::resetPosition(int64_t positionPulses) {
  positionPulses_ = positionPulses;
  if (initialized_) {
    pcnt_counter_clear(static_cast<pcnt_unit_t>(config_.pcntUnit));
  }
}

bool PcntEncoderReader::initialized() const {
  return initialized_;
}
#endif

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
  snapshot_.targetSpeedPercent = 0;
  if (profile_.softStopMs > 0 &&
      (snapshot_.state == MotorState::SoftStarting || snapshot_.state == MotorState::Running)) {
    snapshot_.state = MotorState::SoftStopping;
    stopStartMs_ = snapshot_.lastUpdateMs;
    snapshot_.lastCommandResult = MotorResult::Ok;
    return MotorResult::Ok;
  }
  snapshot_.state = MotorState::Idle;
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

  const uint32_t previousUpdateMs = snapshot_.lastUpdateMs;
  const int64_t previousPosition = snapshot_.positionPulses;
  snapshot_.positionPulses = encoder_->positionPulses();
  snapshot_.encoderDeltaSinceLastCheck = snapshot_.positionPulses - previousPosition;
  if (nowMs > previousUpdateMs) {
    const uint32_t deltaMs = nowMs - previousUpdateMs;
    snapshot_.pulsesPerSecond =
        (snapshot_.encoderDeltaSinceLastCheck * 1000LL) / static_cast<int64_t>(deltaMs);
    if (kinematics_.outputPulsesPerRev > 0) {
      snapshot_.rpm = static_cast<int32_t>(
          (snapshot_.pulsesPerSecond * 60LL) / kinematics_.outputPulsesPerRev);
    } else {
      snapshot_.rpm = 0;
    }
  }
  snapshot_.remainingPulses = snapshot_.targetPulses - snapshot_.positionPulses;
  snapshot_.elapsedMs = nowMs - commandStartMs_;
  snapshot_.lastUpdateMs = nowMs;

  if ((snapshot_.state == MotorState::SoftStarting || snapshot_.state == MotorState::Running ||
       snapshot_.state == MotorState::SoftStopping) &&
      protection_.maxRunPulses > 0 &&
      absoluteDistance(snapshot_.positionPulses, snapshot_.segmentStartPulses) >
          protection_.maxRunPulses) {
    requestEmergencyStop(FaultReason::MaxRunPulses);
    updateTrace(nowMs);
    return;
  }

  if ((snapshot_.state == MotorState::SoftStarting || snapshot_.state == MotorState::Running ||
       snapshot_.state == MotorState::SoftStopping) &&
      protection_.maxRunMs > 0 && snapshot_.elapsedMs > protection_.maxRunMs) {
    requestEmergencyStop(FaultReason::MaxRunMs);
    updateTrace(nowMs);
    return;
  }

  if ((snapshot_.state == MotorState::SoftStarting || snapshot_.state == MotorState::Running) &&
      protection_.stallCheckIntervalMs > 0 && protection_.minPulseDelta > 0 &&
      snapshot_.elapsedMs > protection_.startupGraceMs &&
      nowMs - lastStallCheckMs_ >= protection_.stallCheckIntervalMs) {
    const int64_t delta = absoluteDistance(snapshot_.positionPulses, lastStallCheckPosition_);
    if (delta < protection_.minPulseDelta) {
      requestEmergencyStop(FaultReason::EncoderNoPulse);
      updateTrace(nowMs);
      return;
    }
    lastStallCheckMs_ = nowMs;
    lastStallCheckPosition_ = snapshot_.positionPulses;
  }

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
    int8_t direction = static_cast<int8_t>(snapshot_.direction);
    if (kinematics_.motorDirectionInverted) {
      direction = static_cast<int8_t>(-direction);
    }
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

  if (snapshot_.state == MotorState::SoftStopping) {
    const uint32_t stopElapsedMs = nowMs - stopStartMs_;
    if (profile_.softStopMs == 0 || stopElapsedMs >= profile_.softStopMs) {
      snapshot_.lastCommandResult = driver_->stop(stopPolicy_.emergencyOutputMode);
      snapshot_.state = MotorState::Idle;
      snapshot_.activeCommand = MotorCommand::None;
      snapshot_.direction = MotorDirection::Stopped;
      snapshot_.currentSpeedPercent = 0;
      snapshot_.driverOutputPercent = 0;
      snapshot_.remainingPulses = snapshot_.targetPulses - snapshot_.positionPulses;
      updateTrace(nowMs);
      return;
    }

    const uint32_t scaled =
        static_cast<uint32_t>(profile_.speedPercent) * (profile_.softStopMs - stopElapsedMs) /
        profile_.softStopMs;
    uint8_t output = static_cast<uint8_t>(scaled);
    if (output < profile_.minEffectiveSpeedPercent) {
      output = profile_.minEffectiveSpeedPercent;
    }
    snapshot_.lastCommandResult =
        driver_->setOutput(static_cast<int8_t>(snapshot_.direction), output);
    snapshot_.currentSpeedPercent = output;
    snapshot_.driverOutputPercent = output;
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
  commandStartMs_ = snapshot_.lastUpdateMs;
  lastStallCheckMs_ = commandStartMs_;
  lastStallCheckPosition_ = current;
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

int64_t EncodedDcMotor::absoluteDistance(int64_t lhs, int64_t rhs) {
  return lhs >= rhs ? lhs - rhs : rhs - lhs;
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
