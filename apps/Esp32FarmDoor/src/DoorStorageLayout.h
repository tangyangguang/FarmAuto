#pragma once

#include <cstddef>
#include <cstdint>

#include <Esp32At24cRecordStore.h>

enum class DoorAt24cRecordType : uint16_t {
  RecoveryPolicy = 1,
  State = 2,
  MotionJournal = 3,
  MotionCheckpoint = 4,
  Calibration = 5,
  RecordIndex = 6,
  SystemMeta = 7
};

extern const Esp32At24cRecordStore::RecordStoreConfig kDoorAt24cConfig;
extern const Esp32At24cRecordStore::RegionConfig kDoorAt24cRegions[];
extern const std::size_t kDoorAt24cRegionCount;
extern const uint32_t kDoorAt24cUsedBytes;
