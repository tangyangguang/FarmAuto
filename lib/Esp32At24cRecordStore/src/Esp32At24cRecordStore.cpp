#include "Esp32At24cRecordStore.h"

namespace Esp32At24cRecordStore {

Result RecordStore::begin(IAt24cDevice& device,
                          const RecordStoreConfig& config,
                          const RegionConfig* regions,
                          std::size_t regionCount) {
  if (config.layoutVersion == 0 || config.totalBytes == 0 || config.pageSize == 0 ||
      regions == nullptr || regionCount == 0) {
    return Result::InvalidArgument;
  }

  for (std::size_t i = 0; i < regionCount; ++i) {
    const RegionConfig& region = regions[i];
    if (region.recordType == 0 || region.slotSize == 0 || region.slotCount == 0 ||
        region.schemaVersion == 0) {
      return Result::InvalidArgument;
    }

    const uint32_t regionBytes = static_cast<uint32_t>(region.slotSize) * region.slotCount;
    if (region.offset > config.totalBytes || regionBytes > config.totalBytes - region.offset) {
      return Result::InvalidArgument;
    }
  }

  device_ = &device;
  config_ = config;
  regions_ = regions;
  regionCount_ = regionCount;
  return Result::Ok;
}

Result RecordStore::format() {
  if (!initialized()) {
    return Result::NotInitialized;
  }
  return Result::Ok;
}

Result RecordStore::write(uint16_t recordType, const uint8_t* payload, std::size_t length) {
  if (!initialized()) {
    return Result::NotInitialized;
  }
  if (payload == nullptr && length > 0) {
    return Result::InvalidArgument;
  }
  const RegionConfig* region = findRegion(recordType);
  if (region == nullptr) {
    return Result::RegionNotFound;
  }
  if (length > region->slotSize) {
    return Result::PayloadTooLarge;
  }
  return Result::Ok;
}

Result RecordStore::readLatest(uint16_t recordType,
                               uint8_t* payload,
                               std::size_t capacity,
                               std::size_t& length) {
  length = 0;
  if (!initialized()) {
    return Result::NotInitialized;
  }
  if (payload == nullptr && capacity > 0) {
    return Result::InvalidArgument;
  }
  if (findRegion(recordType) == nullptr) {
    return Result::RegionNotFound;
  }
  return Result::FormatRequired;
}

Result RecordStore::inspect(uint16_t recordType, RecordInspect& out) const {
  if (!initialized()) {
    return Result::NotInitialized;
  }
  const RegionConfig* region = findRegion(recordType);
  if (region == nullptr) {
    return Result::RegionNotFound;
  }
  out = {};
  out.recordType = recordType;
  out.slotCount = region->slotCount;
  out.lastResult = Result::Ok;
  return Result::Ok;
}

bool RecordStore::initialized() const {
  return device_ != nullptr && regions_ != nullptr && regionCount_ > 0;
}

const RegionConfig* RecordStore::findRegion(uint16_t recordType) const {
  for (std::size_t i = 0; i < regionCount_; ++i) {
    if (regions_[i].recordType == recordType) {
      return &regions_[i];
    }
  }
  return nullptr;
}

uint32_t crc32IsoHdlc(const uint8_t* data, std::size_t length) {
  uint32_t crc = 0xFFFFFFFFu;
  for (std::size_t i = 0; i < length; ++i) {
    crc ^= data[i];
    for (uint8_t bit = 0; bit < 8; ++bit) {
      const bool lsb = (crc & 1u) != 0;
      crc >>= 1;
      if (lsb) {
        crc ^= 0xEDB88320u;
      }
    }
  }
  return ~crc;
}

}  // namespace Esp32At24cRecordStore
