#pragma once

#include <cstddef>
#include <cstdint>

#include "DoorController.h"

static constexpr uint32_t kDoorRecoveryMagic = 0xFAD00101u;
static constexpr uint16_t kDoorRecoverySchemaVersion = 1;
static constexpr std::size_t kDoorRecoveryEncodedBytes = 64;

enum class DoorRecoveryCodecResult : uint8_t {
  Ok,
  InvalidArgument,
  BufferTooSmall,
  UnsupportedVersion,
  CrcMismatch
};

struct DoorRecoveryState {
  int64_t positionPulses = 0;
  int64_t openTargetPulses = 0;
  int64_t closedPositionPulses = 0;
  int64_t maxRunPulses = 0;
  uint32_t maxRunMs = 0;
  PositionTrustLevel positionTrustLevel = PositionTrustLevel::Untrusted;
  DoorCommand lastCommand = DoorCommand::None;
  DoorStopReason lastStopReason = DoorStopReason::None;
  uint32_t unixTime = 0;
  uint32_t uptimeSec = 0;
  uint32_t bootId = 0;
};

DoorRecoveryCodecResult encodeDoorRecoveryState(const DoorRecoveryState& state,
                                                uint8_t* out,
                                                std::size_t capacity,
                                                std::size_t& encodedLength);

DoorRecoveryCodecResult verifyDoorRecoveryState(const uint8_t* data, std::size_t length);

DoorRecoveryCodecResult decodeDoorRecoveryState(const uint8_t* data,
                                                std::size_t length,
                                                DoorRecoveryState& out);
