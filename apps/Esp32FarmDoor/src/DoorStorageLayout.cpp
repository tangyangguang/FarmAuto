#include "DoorStorageLayout.h"

namespace {

static constexpr uint16_t kLayoutVersion = 1;
static constexpr uint32_t kAt24c128Bytes = 16UL * 1024UL;
static constexpr uint16_t kAt24c128PageBytes = 64;

Esp32At24cRecordStore::RecordStoreConfig makeStoreConfig() {
  Esp32At24cRecordStore::RecordStoreConfig config;
  config.layoutVersion = kLayoutVersion;
  config.baseAddress = 0;
  config.totalBytes = kAt24c128Bytes;
  config.pageSize = kAt24c128PageBytes;
  config.writeOnlyWhenChanged = true;
  return config;
}

Esp32At24cRecordStore::RegionConfig makeRegion(DoorAt24cRecordType type,
                                                uint32_t offset,
                                                uint16_t slotSize,
                                                uint16_t slotCount) {
  Esp32At24cRecordStore::RegionConfig region;
  region.recordType = static_cast<uint16_t>(type);
  region.offset = offset;
  region.slotSize = slotSize;
  region.slotCount = slotCount;
  region.schemaVersion = 1;
  return region;
}

}  // namespace

const Esp32At24cRecordStore::RecordStoreConfig kDoorAt24cConfig = makeStoreConfig();

const Esp32At24cRecordStore::RegionConfig kDoorAt24cRegions[] = {
    makeRegion(DoorAt24cRecordType::RecoveryPolicy, 0, 256, 4),
    makeRegion(DoorAt24cRecordType::State, 1024, 128, 16),
    makeRegion(DoorAt24cRecordType::MotionJournal, 3072, 128, 8),
    makeRegion(DoorAt24cRecordType::MotionCheckpoint, 4096, 128, 32),
    makeRegion(DoorAt24cRecordType::Calibration, 8192, 256, 4),
    makeRegion(DoorAt24cRecordType::RecordIndex, 9216, 512, 4),
    makeRegion(DoorAt24cRecordType::SystemMeta, 11264, 128, 4),
};

const std::size_t kDoorAt24cRegionCount =
    sizeof(kDoorAt24cRegions) / sizeof(kDoorAt24cRegions[0]);

const uint32_t kDoorAt24cUsedBytes = 11776;
