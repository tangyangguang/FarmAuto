#pragma once

#include <cstdint>

namespace Esp32EncodedDcMotor {

enum class MotorState : uint8_t {
  Idle,
  SoftStarting,
  Running,
  SoftStopping,
  Braking,
  Fault
};

enum class MotorResult : uint8_t {
  Ok,
  Busy,
  InvalidArgument,
  InvalidState,
  NotInitialized,
  AlreadyAtTarget,
  FaultActive,
  ConfigMissing,
  TargetTooSmall,
  DriverRejected
};

enum class CountMode : uint8_t {
  X1,
  X2,
  X4
};

enum class DriverType : uint8_t {
  SinglePwm,
  At8236HBridge
};

enum class EncoderBackendType : uint8_t {
  External,
  Pcnt
};

enum class MotorCommand : uint8_t {
  None,
  MovePulses,
  MoveToPosition,
  Stop,
  EmergencyStop
};

enum class MotorDirection : int8_t {
  Reverse = -1,
  Stopped = 0,
  Forward = 1
};

enum class StopMode : uint8_t {
  Coast,
  Brake,
  SoftStopThenBrake,
  EmergencyStop
};

enum class EmergencyOutputMode : uint8_t {
  Coast,
  Brake
};

enum class FaultReason : uint8_t {
  None,
  ExternalFault,
  MaxRunPulses,
  MaxRunMs,
  EncoderNoPulse,
  DriverRejected
};

struct MotorHardwareConfig {
  DriverType driverType = DriverType::SinglePwm;
  uint32_t pwmFrequencyHz = 20000;
  uint8_t pwmResolutionBits = 10;
  bool pwmPolarity = true;
  int8_t ledcChannelA = -1;
  int8_t ledcChannelB = -1;
};

struct EncoderBackendConfig {
  EncoderBackendType backendType = EncoderBackendType::External;
  int8_t pinA = -1;
  int8_t pinB = -1;
  CountMode countMode = CountMode::X1;
  int8_t pcntUnit = -1;
  int8_t pcntChannelA = -1;
  int8_t pcntChannelB = -1;
  uint32_t glitchFilterNs = 0;
};

struct MotorKinematics {
  uint16_t motorShaftPulsesPerRev = 16;
  float gearRatio = 1.0f;
  int64_t outputPulsesPerRev = 0;
  CountMode countMode = CountMode::X1;
  bool motorDirectionInverted = false;
  bool encoderDirectionInverted = false;
};

struct MotorMotionProfile {
  uint8_t speedPercent = 60;
  uint32_t softStartMs = 1000;
  uint32_t softStopMs = 500;
  uint8_t minEffectiveSpeedPercent = 15;
  uint32_t controlTickMs = 20;
};

struct MotorProtection {
  uint32_t startupGraceMs = 1000;
  uint32_t stallCheckIntervalMs = 250;
  int64_t minPulseDelta = 1;
  uint32_t maxRunMs = 0;
  int64_t maxRunPulses = 0;
};

struct MotorStopPolicy {
  StopMode normalStopMode = StopMode::SoftStopThenBrake;
  StopMode faultStopMode = StopMode::EmergencyStop;
  uint32_t brakeMs = 100;
  EmergencyOutputMode emergencyOutputMode = EmergencyOutputMode::Coast;
};

struct MotorSnapshot {
  MotorState state = MotorState::Idle;
  MotorCommand activeCommand = MotorCommand::None;
  MotorDirection direction = MotorDirection::Stopped;
  uint8_t currentSpeedPercent = 0;
  uint8_t targetSpeedPercent = 0;
  uint8_t driverOutputPercent = 0;
  int64_t positionPulses = 0;
  int64_t segmentStartPulses = 0;
  int64_t targetPulses = 0;
  int64_t remainingPulses = 0;
  int64_t pulsesPerSecond = 0;
  int32_t rpm = 0;
  uint32_t elapsedMs = 0;
  uint32_t lastUpdateMs = 0;
  uint32_t lastPulseMs = 0;
  FaultReason faultReason = FaultReason::None;
  MotorResult lastCommandResult = MotorResult::Ok;
  int64_t encoderDeltaSinceLastCheck = 0;
};

struct MotorTracePoint {
  uint32_t timestampMs = 0;
  MotorState state = MotorState::Idle;
  MotorCommand activeCommand = MotorCommand::None;
  MotorDirection direction = MotorDirection::Stopped;
  int64_t positionPulses = 0;
  int64_t targetPulses = 0;
  int64_t remainingPulses = 0;
  int64_t pulsesPerSecond = 0;
  int32_t rpm = 0;
  uint8_t targetSpeedPercent = 0;
  uint8_t driverOutputPercent = 0;
  int64_t encoderDelta = 0;
  FaultReason faultReason = FaultReason::None;
};

class IMotorDriver {
public:
  virtual ~IMotorDriver() = default;
  virtual MotorResult setOutput(int8_t direction, uint8_t percent) = 0;
  virtual MotorResult stop(EmergencyOutputMode mode) = 0;
};

class IEncoderReader {
public:
  virtual ~IEncoderReader() = default;
  virtual int64_t positionPulses() const = 0;
};

class EncodedDcMotor {
public:
  MotorResult begin(IMotorDriver& driver,
                    IEncoderReader& encoder,
                    const MotorHardwareConfig& hardware,
                    const EncoderBackendConfig& encoderConfig);
  MotorResult configure(const MotorKinematics& kinematics,
                        const MotorMotionProfile& profile,
                        const MotorProtection& protection,
                        const MotorStopPolicy& stopPolicy);

  MotorResult requestMovePulses(int64_t pulses);
  MotorResult requestMoveToPosition(int64_t positionPulses);
  MotorResult requestStop();
  MotorResult requestEmergencyStop(FaultReason reason);
  void setCurrentPositionPulses(int64_t positionPulses);
  void update(uint32_t nowMs);

  MotorSnapshot snapshot() const;
  MotorTracePoint latestTracePoint() const;

private:
  MotorResult startMoveTo(int64_t targetPulses);
  uint8_t outputForElapsed(uint32_t elapsedMs) const;
  static int64_t absoluteDistance(int64_t lhs, int64_t rhs);
  void updateTrace(uint32_t nowMs);

  IMotorDriver* driver_ = nullptr;
  IEncoderReader* encoder_ = nullptr;
  MotorHardwareConfig hardware_{};
  EncoderBackendConfig encoderConfig_{};
  MotorKinematics kinematics_{};
  MotorMotionProfile profile_{};
  MotorProtection protection_{};
  MotorStopPolicy stopPolicy_{};
  MotorSnapshot snapshot_{};
  MotorTracePoint trace_{};
  bool configured_ = false;
  uint32_t commandStartMs_ = 0;
  uint32_t stopStartMs_ = 0;
};

}  // namespace Esp32EncodedDcMotor
