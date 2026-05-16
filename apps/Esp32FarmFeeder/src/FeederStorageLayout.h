#pragma once

#include <cstddef>
#include <cstdint>

#include <Esp32At24cRecordStore.h>

#include "FeederBucketCodec.h"
#include "FeederScheduleCodec.h"
#include "FeederTargetCodec.h"

enum class FeederAt24cRecordType : uint16_t {
  RecoveryPolicy = 1,
  Today = 2,
  Schedule = 3,
  ChannelTarget = 4,
  BucketState = 5,
  Calibration = 6,
  RecordIndex = 7,
  SystemMeta = 8
};

extern const Esp32At24cRecordStore::RecordStoreConfig kFeederAt24cConfig;
extern const Esp32At24cRecordStore::RegionConfig kFeederAt24cRegions[];
extern const std::size_t kFeederAt24cRegionCount;
extern const uint32_t kFeederAt24cUsedBytes;
