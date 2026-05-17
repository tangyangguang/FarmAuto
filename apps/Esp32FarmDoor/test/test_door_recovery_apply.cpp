#include <cassert>

#include "DoorRecoveryApply.h"

int main() {
  DoorController door;

  DoorRecoveryState state;
  state.positionPulses = 100;
  state.closedPositionPulses = 0;
  state.openTargetPulses = 1000;
  state.maxRunPulses = 1500;
  state.maxRunMs = 25000;
  state.positionTrustLevel = PositionTrustLevel::Trusted;

  assert(applyDoorRecoveryState(door, state) == DoorCommandResult::Ok);
  DoorSnapshot snapshot = door.snapshot();
  assert(snapshot.positionPulses == 100);
  assert(snapshot.openTargetPulses == 1000);
  assert(snapshot.closedPositionPulses == 0);
  assert(snapshot.positionTrustLevel == PositionTrustLevel::Trusted);
  assert(snapshot.state == DoorState::IdlePartial);

  state.positionPulses = 1000;
  assert(applyDoorRecoveryState(door, state) == DoorCommandResult::Ok);
  snapshot = door.snapshot();
  assert(snapshot.state == DoorState::IdleOpen);

  state.openTargetPulses = 0;
  assert(applyDoorRecoveryState(door, state) == DoorCommandResult::InvalidArgument);

  DoorControllerConfig config;
  config.closedPositionPulses = 0;
  config.openTargetPulses = 1200;
  config.maxRunPulses = 1800;
  config.maxRunMs = 30000;
  assert(door.configure(config) == DoorCommandResult::Ok);
  assert(door.markPositionClosed() == DoorCommandResult::Ok);

  DoorRecoveryState generated = doorRecoveryStateFromSnapshot(door.snapshot(), config, 111, 222, 3);
  assert(generated.positionPulses == 0);
  assert(generated.openTargetPulses == 1200);
  assert(generated.maxRunPulses == 1800);
  assert(generated.maxRunMs == 30000);
  assert(generated.positionTrustLevel == PositionTrustLevel::Trusted);
  assert(generated.unixTime == 111);
  assert(generated.uptimeSec == 222);
  assert(generated.bootId == 3);

  return 0;
}
