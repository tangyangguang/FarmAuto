#include <cassert>
#include <cstddef>
#include <cstdint>

#include "DoorRecoveryCodec.h"

int main() {
  DoorRecoveryState state;
  state.positionPulses = 5200;
  state.openTargetPulses = 10480;
  state.closedPositionPulses = 0;
  state.maxRunPulses = 15720;
  state.maxRunMs = 25000;
  state.positionTrustLevel = PositionTrustLevel::Limited;
  state.lastCommand = DoorCommand::Open;
  state.lastStopReason = DoorStopReason::ProtectiveStop;
  state.unixTime = 1800000000;
  state.uptimeSec = 123;
  state.bootId = 9;

  uint8_t encoded[kDoorRecoveryEncodedBytes] = {};
  std::size_t encodedLength = 0;
  assert(encodeDoorRecoveryState(state, encoded, sizeof(encoded), encodedLength) ==
         DoorRecoveryCodecResult::Ok);
  assert(encodedLength == kDoorRecoveryEncodedBytes);
  assert(verifyDoorRecoveryState(encoded, encodedLength) == DoorRecoveryCodecResult::Ok);

  DoorRecoveryState decoded;
  assert(decodeDoorRecoveryState(encoded, encodedLength, decoded) ==
         DoorRecoveryCodecResult::Ok);
  assert(decoded.positionPulses == state.positionPulses);
  assert(decoded.openTargetPulses == state.openTargetPulses);
  assert(decoded.maxRunPulses == state.maxRunPulses);
  assert(decoded.maxRunMs == state.maxRunMs);
  assert(decoded.positionTrustLevel == PositionTrustLevel::Limited);
  assert(decoded.lastCommand == DoorCommand::Open);
  assert(decoded.lastStopReason == DoorStopReason::ProtectiveStop);
  assert(decoded.unixTime == state.unixTime);
  assert(decoded.uptimeSec == state.uptimeSec);
  assert(decoded.bootId == state.bootId);

  encoded[20] ^= 0x7F;
  assert(verifyDoorRecoveryState(encoded, encodedLength) == DoorRecoveryCodecResult::CrcMismatch);

  state.openTargetPulses = 0;
  assert(encodeDoorRecoveryState(state, encoded, sizeof(encoded), encodedLength) ==
         DoorRecoveryCodecResult::InvalidArgument);

  return 0;
}
