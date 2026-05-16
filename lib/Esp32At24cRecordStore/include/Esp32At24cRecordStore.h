#pragma once

#include <cstddef>
#include <cstdint>

namespace Esp32At24cRecordStore {

enum class Result : uint8_t {
  Ok,
  Unchanged,
  InvalidArgument,
  NotInitialized,
  DeviceOffline,
  PayloadTooLarge,
  RegionNotFound,
  CrcMismatch,
  LayoutMismatch,
  WriteFailed,
  VerifyFailed,
  FormatRequired
};

enum class SlotState : uint8_t {
  Empty,
  Writing,
  Valid
};

class IAt24cDevice {
public:
  virtual ~IAt24cDevice() = default;
  virtual bool read(uint32_t address, uint8_t* data, std::size_t length) = 0;
  virtual bool write(uint32_t address, const uint8_t* data, std::size_t length) = 0;
};

struct RecordStoreConfig {
  uint16_t layoutVersion = 1;
  uint32_t baseAddress = 0;
  uint32_t totalBytes = 0;
  uint16_t pageSize = 64;
  bool writeOnlyWhenChanged = true;
};

struct RegionConfig {
  uint16_t recordType = 0;
  uint32_t offset = 0;
  uint16_t slotSize = 0;
  uint16_t slotCount = 0;
  uint16_t schemaVersion = 1;
};

struct RecordInspect {
  uint16_t recordType = 0;
  uint16_t validSlots = 0;
  uint16_t slotCount = 0;
  uint32_t latestSequence = 0;
  uint32_t estimatedWrites = 0;
  Result lastResult = Result::NotInitialized;
};

class RecordStore {
public:
  Result begin(IAt24cDevice& device,
               const RecordStoreConfig& config,
               const RegionConfig* regions,
               std::size_t regionCount);

  Result format();
  Result write(uint16_t recordType, const uint8_t* payload, std::size_t length);
  Result readLatest(uint16_t recordType, uint8_t* payload, std::size_t capacity, std::size_t& length);
  Result inspect(uint16_t recordType, RecordInspect& out) const;

  bool initialized() const;

private:
  const RegionConfig* findRegion(uint16_t recordType) const;

  IAt24cDevice* device_ = nullptr;
  RecordStoreConfig config_{};
  const RegionConfig* regions_ = nullptr;
  std::size_t regionCount_ = 0;
};

uint32_t crc32IsoHdlc(const uint8_t* data, std::size_t length);

}  // namespace Esp32At24cRecordStore
