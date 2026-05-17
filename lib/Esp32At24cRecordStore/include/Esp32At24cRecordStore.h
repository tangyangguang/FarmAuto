#pragma once

#include <cstddef>
#include <cstdint>

#if defined(ARDUINO)
#include <Arduino.h>
#include <Wire.h>
#endif

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

class II2cBus {
public:
  virtual ~II2cBus() = default;
  virtual bool write(uint8_t deviceAddress, const uint8_t* data, std::size_t length) = 0;
  virtual bool writeRead(uint8_t deviceAddress,
                         const uint8_t* writeData,
                         std::size_t writeLength,
                         uint8_t* readData,
                         std::size_t readLength) = 0;
  virtual void delayMs(uint16_t ms) = 0;
};

struct At24cI2cDeviceConfig {
  uint8_t deviceAddress = 0x50;
  uint32_t totalBytes = 16UL * 1024UL;
  uint16_t pageSize = 64;
  uint8_t addressBytes = 2;
  uint8_t maxTransferBytes = 32;
  uint8_t writePollAttempts = 20;
  uint16_t writePollDelayMs = 5;
};

class At24cI2cDevice : public IAt24cDevice {
public:
  At24cI2cDevice(II2cBus& bus, const At24cI2cDeviceConfig& config);

  bool read(uint32_t address, uint8_t* data, std::size_t length) override;
  bool write(uint32_t address, const uint8_t* data, std::size_t length) override;

private:
  bool validConfig() const;
  bool inRange(uint32_t address, std::size_t length) const;
  bool writeAddress(uint32_t address, uint8_t* out) const;
  bool waitForWriteComplete();

  II2cBus& bus_;
  At24cI2cDeviceConfig config_{};
};

#if defined(ARDUINO)
class ArduinoWireI2cBus : public II2cBus {
public:
  explicit ArduinoWireI2cBus(TwoWire& wire = Wire);

  bool write(uint8_t deviceAddress, const uint8_t* data, std::size_t length) override;
  bool writeRead(uint8_t deviceAddress,
                 const uint8_t* writeData,
                 std::size_t writeLength,
                 uint8_t* readData,
                 std::size_t readLength) override;
  void delayMs(uint16_t ms) override;

private:
  TwoWire& wire_;
};
#endif

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
  uint16_t latestSlotIndex = 0;
  uint32_t latestSequence = 0;
  uint32_t estimatedWrites = 0;
  Result lastResult = Result::NotInitialized;
};

struct SlotHeader {
  uint32_t magic = 0;
  uint16_t layoutVersion = 0;
  uint16_t recordType = 0;
  uint16_t schemaVersion = 0;
  uint8_t flags = 0;
  uint32_t sequence = 0;
  uint16_t payloadLength = 0;
  uint32_t headerCrc = 0;
  uint32_t payloadCrc = 0;
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
  bool readHeader(const RegionConfig& region, uint16_t slotIndex, SlotHeader& out) const;
  bool validHeaderForRegion(const RegionConfig& region, const SlotHeader& header) const;
  bool readLatestHeader(const RegionConfig& region, SlotHeader& out, uint16_t& slotIndex) const;

  IAt24cDevice* device_ = nullptr;
  RecordStoreConfig config_{};
  const RegionConfig* regions_ = nullptr;
  std::size_t regionCount_ = 0;
};

uint32_t crc32IsoHdlc(const uint8_t* data, std::size_t length);

}  // namespace Esp32At24cRecordStore
