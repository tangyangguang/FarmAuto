#pragma once

#include <cstdint>

enum class DoorState : uint8_t {
  PositionUnknown,
  IdleClosed,
  IdleOpen,
  IdlePartial,
  Opening,
  Closing,
  Fault
};

enum class DoorCommand : uint8_t {
  None,
  Open,
  Close,
  Stop
};

enum class DoorCommandResult : uint8_t {
  Ok,
  Busy,
  InvalidArgument,
  PositionUntrusted,
  FaultActive
};

enum class PositionTrustLevel : uint8_t {
  Trusted,
  Limited,
  Untrusted
};

enum class DoorStopReason : uint8_t {
  None,
  TargetReached,
  UserStop,
  ProtectiveStop,
  FaultStop
};

enum class DoorFaultReason : uint8_t {
  None,
  InvalidConfig,
  MotorFault,
  PositionLost
};

struct DoorControllerConfig {
  int64_t openTargetPulses = 0;
  int64_t closedPositionPulses = 0;
  int64_t maxRunPulses = 0;
  uint32_t maxRunMs = 0;
};

struct DoorSnapshot {
  DoorState state = DoorState::PositionUnknown;
  DoorCommand activeCommand = DoorCommand::None;
  PositionTrustLevel positionTrustLevel = PositionTrustLevel::Untrusted;
  DoorStopReason lastStopReason = DoorStopReason::None;
  DoorFaultReason faultReason = DoorFaultReason::None;
  int64_t positionPulses = 0;
  int64_t targetPulses = 0;
  int64_t openTargetPulses = 0;
  int64_t closedPositionPulses = 0;
};

class DoorController {
 public:
  DoorCommandResult configure(const DoorControllerConfig& config);

  DoorCommandResult markPositionClosed();
  DoorCommandResult markPositionOpen();
  DoorCommandResult setTrustedPosition(int64_t positionPulses, PositionTrustLevel trustLevel);

  DoorCommandResult requestOpen();
  DoorCommandResult requestClose();
  DoorCommandResult requestStop(int64_t stoppedPositionPulses);
  DoorCommandResult onMotionTargetReached(int64_t finalPositionPulses);
  DoorCommandResult enterFault(DoorFaultReason reason);

  DoorSnapshot snapshot() const;

 private:
  bool configured() const;
  bool isRunning() const;
  DoorCommandResult requestMove(DoorCommand command, DoorState runningState, int64_t targetPulses);

  DoorControllerConfig config_;
  DoorSnapshot snapshot_;
};
