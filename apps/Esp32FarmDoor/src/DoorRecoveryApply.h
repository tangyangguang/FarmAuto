#pragma once

#include <cstdint>

#include "DoorController.h"
#include "DoorRecoveryCodec.h"

DoorCommandResult applyDoorRecoveryState(DoorController& door, const DoorRecoveryState& state);

DoorRecoveryState doorRecoveryStateFromSnapshot(const DoorSnapshot& snapshot,
                                                const DoorControllerConfig& config,
                                                uint32_t unixTime,
                                                uint32_t uptimeSec,
                                                uint32_t bootId);
