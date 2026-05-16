#include "FeederStorageLayout.h"

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

Esp32At24cRecordStore::RegionConfig makeRegion(FeederAt24cRecordType type,
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

const Esp32At24cRecordStore::RecordStoreConfig kFeederAt24cConfig = makeStoreConfig();

const Esp32At24cRecordStore::RegionConfig kFeederAt24cRegions[] = {
    makeRegion(FeederAt24cRecordType::RecoveryPolicy, 0, 512, 4),
    makeRegion(FeederAt24cRecordType::Today, 2048, 256, 16),
    makeRegion(FeederAt24cRecordType::Schedule, 6144, 512, 4),
    makeRegion(FeederAt24cRecordType::ChannelTarget, 8192, 256, 4),
    makeRegion(FeederAt24cRecordType::BucketState, 9216, 256, 4),
    makeRegion(FeederAt24cRecordType::Calibration, 10240, 256, 4),
    makeRegion(FeederAt24cRecordType::RecordIndex, 11264, 512, 4),
    makeRegion(FeederAt24cRecordType::SystemMeta, 13312, 128, 4),
};

const std::size_t kFeederAt24cRegionCount =
    sizeof(kFeederAt24cRegions) / sizeof(kFeederAt24cRegions[0]);

const uint32_t kFeederAt24cUsedBytes = 13824;
