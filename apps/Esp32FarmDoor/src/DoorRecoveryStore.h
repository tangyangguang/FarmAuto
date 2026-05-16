#pragma once

#include <Esp32At24cRecordStore.h>

#include "DoorRecoveryCodec.h"

Esp32At24cRecordStore::Result saveDoorRecoveryState(
    Esp32At24cRecordStore::RecordStore& store,
    const DoorRecoveryState& state);

Esp32At24cRecordStore::Result loadDoorRecoveryState(Esp32At24cRecordStore::RecordStore& store,
                                                    DoorRecoveryState& out);
