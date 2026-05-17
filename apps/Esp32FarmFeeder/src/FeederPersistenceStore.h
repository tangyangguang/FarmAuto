#pragma once

#include <Esp32At24cRecordStore.h>

#include "FeederBucketCodec.h"
#include "FeederCalibrationCodec.h"
#include "FeederScheduleCodec.h"
#include "FeederTargetCodec.h"

Esp32At24cRecordStore::Result saveFeederSchedule(
    Esp32At24cRecordStore::RecordStore& store,
    const FeederScheduleSnapshot& snapshot);

Esp32At24cRecordStore::Result loadFeederSchedule(Esp32At24cRecordStore::RecordStore& store,
                                                 FeederScheduleSnapshot& out);

Esp32At24cRecordStore::Result saveFeederTargets(Esp32At24cRecordStore::RecordStore& store,
                                                const FeederTargetSnapshot& snapshot);

Esp32At24cRecordStore::Result loadFeederTargets(Esp32At24cRecordStore::RecordStore& store,
                                                FeederTargetSnapshot& out);

Esp32At24cRecordStore::Result saveFeederBuckets(Esp32At24cRecordStore::RecordStore& store,
                                                const FeederBucketSnapshot& snapshot);

Esp32At24cRecordStore::Result loadFeederBuckets(Esp32At24cRecordStore::RecordStore& store,
                                                FeederBucketSnapshot& out);

Esp32At24cRecordStore::Result saveFeederCalibration(
    Esp32At24cRecordStore::RecordStore& store,
    const FeederBucketSnapshot& snapshot);

Esp32At24cRecordStore::Result loadFeederCalibration(Esp32At24cRecordStore::RecordStore& store,
                                                    FeederBucketSnapshot& out);
