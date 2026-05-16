#include <cassert>

#include "DoorController.h"

int main() {
  DoorController door;

  assert(door.requestOpen() == DoorCommandResult::InvalidArgument);

  DoorControllerConfig config;
  config.openTargetPulses = 1000;
  config.maxRunPulses = 1500;
  config.maxRunMs = 25000;
  assert(door.configure(config) == DoorCommandResult::Ok);

  assert(door.requestOpen() == DoorCommandResult::PositionUntrusted);

  assert(door.markPositionClosed() == DoorCommandResult::Ok);
  assert(door.snapshot().state == DoorState::IdleClosed);
  assert(door.snapshot().positionTrustLevel == PositionTrustLevel::Trusted);

  assert(door.requestOpen() == DoorCommandResult::Ok);
  assert(door.snapshot().state == DoorState::Opening);
  assert(door.snapshot().activeCommand == DoorCommand::Open);
  assert(door.snapshot().targetPulses == 1000);

  assert(door.requestClose() == DoorCommandResult::Busy);

  assert(door.onMotionTargetReached(1000) == DoorCommandResult::Ok);
  assert(door.snapshot().state == DoorState::IdleOpen);
  assert(door.snapshot().activeCommand == DoorCommand::None);

  assert(door.requestClose() == DoorCommandResult::Ok);
  assert(door.snapshot().state == DoorState::Closing);
  assert(door.snapshot().targetPulses == 0);

  assert(door.requestStop(500) == DoorCommandResult::Ok);
  assert(door.snapshot().state == DoorState::IdlePartial);
  assert(door.snapshot().positionPulses == 500);
  assert(door.snapshot().lastStopReason == DoorStopReason::UserStop);

  assert(door.updateTravel(1200, 1800) == DoorCommandResult::Ok);
  assert(door.snapshot().state == DoorState::IdlePartial);
  assert(door.snapshot().openTargetPulses == 1200);

  assert(door.enterFault(DoorFaultReason::PositionLost) == DoorCommandResult::Ok);
  assert(door.snapshot().state == DoorState::Fault);
  assert(door.requestOpen() == DoorCommandResult::FaultActive);
  assert(door.updateTravel(1300, 1950) == DoorCommandResult::FaultActive);

  assert(door.clearFault() == DoorCommandResult::Ok);
  assert(door.snapshot().state == DoorState::IdlePartial);
  assert(door.snapshot().faultReason == DoorFaultReason::None);

  return 0;
}
