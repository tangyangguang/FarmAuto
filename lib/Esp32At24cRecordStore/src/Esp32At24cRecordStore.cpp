#include "Esp32At24cRecordStore.h"

#include <cstring>

namespace Esp32At24cRecordStore {

namespace {

constexpr uint32_t kMagic = 0xFA24C001u;
constexpr uint8_t kFlagWriting = 0x01u;
constexpr uint8_t kFlagValid = 0x02u;
constexpr std::size_t kHeaderSize = 32;
constexpr std::size_t kMaxI2cTransferBytes = 64;

uint16_t readU16(const uint8_t* bytes) {
  return static_cast<uint16_t>(bytes[0]) | (static_cast<uint16_t>(bytes[1]) << 8);
}

uint32_t readU32(const uint8_t* bytes) {
  return static_cast<uint32_t>(bytes[0]) | (static_cast<uint32_t>(bytes[1]) << 8) |
         (static_cast<uint32_t>(bytes[2]) << 16) | (static_cast<uint32_t>(bytes[3]) << 24);
}

void writeU16(uint8_t* bytes, uint16_t value) {
  bytes[0] = static_cast<uint8_t>(value & 0xFFu);
  bytes[1] = static_cast<uint8_t>((value >> 8) & 0xFFu);
}

void writeU32(uint8_t* bytes, uint32_t value) {
  bytes[0] = static_cast<uint8_t>(value & 0xFFu);
  bytes[1] = static_cast<uint8_t>((value >> 8) & 0xFFu);
  bytes[2] = static_cast<uint8_t>((value >> 16) & 0xFFu);
  bytes[3] = static_cast<uint8_t>((value >> 24) & 0xFFu);
}

bool sequenceAfter(uint32_t candidate, uint32_t current) {
  return candidate != current && static_cast<uint32_t>(candidate - current) < 0x80000000u;
}

uint32_t slotAddress(const RecordStoreConfig& config,
                     const RegionConfig& region,
                     uint16_t slotIndex) {
  return config.baseAddress + region.offset +
         static_cast<uint32_t>(region.slotSize) * slotIndex;
}

void serializeHeader(const SlotHeader& header, uint8_t* bytes) {
  std::memset(bytes, 0, kHeaderSize);
  writeU32(bytes + 0, header.magic);
  writeU16(bytes + 4, header.layoutVersion);
  writeU16(bytes + 6, header.recordType);
  writeU16(bytes + 8, header.schemaVersion);
  bytes[10] = header.flags;
  bytes[11] = 0;
  writeU32(bytes + 12, header.sequence);
  writeU16(bytes + 16, header.payloadLength);
  writeU16(bytes + 18, static_cast<uint16_t>(kHeaderSize));
  writeU32(bytes + 20, header.headerCrc);
  writeU32(bytes + 24, header.payloadCrc);
}

SlotHeader deserializeHeader(const uint8_t* bytes) {
  SlotHeader header;
  header.magic = readU32(bytes + 0);
  header.layoutVersion = readU16(bytes + 4);
  header.recordType = readU16(bytes + 6);
  header.schemaVersion = readU16(bytes + 8);
  header.flags = bytes[10];
  header.sequence = readU32(bytes + 12);
  header.payloadLength = readU16(bytes + 16);
  header.headerCrc = readU32(bytes + 20);
  header.payloadCrc = readU32(bytes + 24);
  return header;
}

uint32_t headerCrcFor(SlotHeader header) {
  header.headerCrc = 0;
  uint8_t bytes[kHeaderSize];
  serializeHeader(header, bytes);
  return crc32IsoHdlc(bytes, kHeaderSize);
}

}  // namespace

At24cI2cDevice::At24cI2cDevice(II2cBus& bus, const At24cI2cDeviceConfig& config)
    : bus_(bus), config_(config) {}

bool At24cI2cDevice::read(uint32_t address, uint8_t* data, std::size_t length) {
  if (data == nullptr || !validConfig() || !inRange(address, length)) {
    return false;
  }
  std::size_t read = 0;
  while (read < length) {
    const std::size_t chunk =
        (length - read) < config_.maxTransferBytes ? (length - read) : config_.maxTransferBytes;
    uint8_t addressBytes[2] = {};
    if (!writeAddress(address + read, addressBytes) ||
        !bus_.writeRead(config_.deviceAddress,
                        addressBytes,
                        config_.addressBytes,
                        data + read,
                        chunk)) {
      return false;
    }
    read += chunk;
  }
  return true;
}

bool At24cI2cDevice::write(uint32_t address, const uint8_t* data, std::size_t length) {
  if ((data == nullptr && length > 0) || !validConfig() || !inRange(address, length)) {
    return false;
  }
  std::size_t written = 0;
  while (written < length) {
    const uint32_t currentAddress = address + written;
    const std::size_t pageRemaining = config_.pageSize - (currentAddress % config_.pageSize);
    const std::size_t dataCapacity = config_.maxTransferBytes - config_.addressBytes;
    std::size_t chunk = length - written;
    if (chunk > pageRemaining) {
      chunk = pageRemaining;
    }
    if (chunk > dataCapacity) {
      chunk = dataCapacity;
    }

    uint8_t buffer[kMaxI2cTransferBytes] = {};
    if (!writeAddress(currentAddress, buffer)) {
      return false;
    }
    std::memcpy(buffer + config_.addressBytes, data + written, chunk);
    if (!bus_.write(config_.deviceAddress, buffer, config_.addressBytes + chunk)) {
      return false;
    }
    if (!waitForWriteComplete()) {
      return false;
    }
    written += chunk;
  }
  return true;
}

bool At24cI2cDevice::validConfig() const {
  return config_.totalBytes > 0 && config_.pageSize > 0 && config_.addressBytes == 2 &&
         config_.maxTransferBytes > config_.addressBytes &&
         config_.maxTransferBytes <= kMaxI2cTransferBytes && config_.writePollAttempts > 0;
}

bool At24cI2cDevice::inRange(uint32_t address, std::size_t length) const {
  return address <= config_.totalBytes && length <= config_.totalBytes - address;
}

bool At24cI2cDevice::writeAddress(uint32_t address, uint8_t* out) const {
  if (out == nullptr || address > 0xFFFFu) {
    return false;
  }
  out[0] = static_cast<uint8_t>((address >> 8) & 0xFFu);
  out[1] = static_cast<uint8_t>(address & 0xFFu);
  return true;
}

bool At24cI2cDevice::waitForWriteComplete() {
  for (uint8_t attempt = 0; attempt < config_.writePollAttempts; ++attempt) {
    bus_.delayMs(config_.writePollDelayMs);
    if (bus_.write(config_.deviceAddress, nullptr, 0)) {
      return true;
    }
  }
  return false;
}

#if defined(ARDUINO)
ArduinoWireI2cBus::ArduinoWireI2cBus(TwoWire& wire) : wire_(wire) {}

bool ArduinoWireI2cBus::write(uint8_t deviceAddress, const uint8_t* data, std::size_t length) {
  wire_.beginTransmission(deviceAddress);
  for (std::size_t i = 0; i < length; ++i) {
    wire_.write(data[i]);
  }
  return wire_.endTransmission() == 0;
}

bool ArduinoWireI2cBus::writeRead(uint8_t deviceAddress,
                                  const uint8_t* writeData,
                                  std::size_t writeLength,
                                  uint8_t* readData,
                                  std::size_t readLength) {
  if ((writeData == nullptr && writeLength > 0) || (readData == nullptr && readLength > 0)) {
    return false;
  }
  wire_.beginTransmission(deviceAddress);
  for (std::size_t i = 0; i < writeLength; ++i) {
    wire_.write(writeData[i]);
  }
  if (wire_.endTransmission(false) != 0) {
    return false;
  }
  const std::size_t received = wire_.requestFrom(static_cast<int>(deviceAddress),
                                                 static_cast<int>(readLength));
  if (received != readLength) {
    return false;
  }
  for (std::size_t i = 0; i < readLength; ++i) {
    readData[i] = static_cast<uint8_t>(wire_.read());
  }
  return true;
}

void ArduinoWireI2cBus::delayMs(uint16_t ms) {
  delay(ms);
}
#endif

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
    if (region.slotSize <= kHeaderSize) {
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
  uint8_t erased[32];
  std::memset(erased, 0xFF, sizeof(erased));
  for (std::size_t regionIndex = 0; regionIndex < regionCount_; ++regionIndex) {
    const RegionConfig& region = regions_[regionIndex];
    const uint32_t start = config_.baseAddress + region.offset;
    const uint32_t length = static_cast<uint32_t>(region.slotSize) * region.slotCount;
    uint32_t written = 0;
    while (written < length) {
      const std::size_t chunk =
          (length - written) < sizeof(erased) ? (length - written) : sizeof(erased);
      if (!device_->write(start + written, erased, chunk)) {
        return Result::WriteFailed;
      }
      written += static_cast<uint32_t>(chunk);
    }
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
  if (length > region->slotSize - kHeaderSize) {
    return Result::PayloadTooLarge;
  }

  RecordInspect currentInspect;
  const Result inspectResult = inspect(recordType, currentInspect);
  if (inspectResult != Result::Ok) {
    return inspectResult;
  }

  std::size_t latestLength = 0;
  uint8_t compareBuffer[32];
  bool samePayload = config_.writeOnlyWhenChanged && currentInspect.validSlots > 0;
  if (samePayload) {
    SlotHeader latestHeader;
    uint16_t latestSlot = 0;
    if (!readLatestHeader(*region, latestHeader, latestSlot)) {
      samePayload = false;
    } else if (latestHeader.payloadLength != length) {
      samePayload = false;
    } else {
      while (latestLength < length) {
        const std::size_t chunk =
            (length - latestLength) < sizeof(compareBuffer) ? (length - latestLength)
                                                            : sizeof(compareBuffer);
        if (!device_->read(slotAddress(config_, *region, latestSlot) + kHeaderSize + latestLength,
                           compareBuffer,
                           chunk)) {
          return Result::DeviceOffline;
        }
        if (std::memcmp(compareBuffer, payload + latestLength, chunk) != 0) {
          samePayload = false;
          break;
        }
        latestLength += chunk;
      }
    }
  }
  if (samePayload) {
    return Result::Unchanged;
  }

  const uint16_t targetSlot = currentInspect.validSlots == 0
                                  ? 0
                                  : static_cast<uint16_t>((currentInspect.latestSlotIndex + 1) %
                                                          region->slotCount);
  const uint32_t targetAddress = slotAddress(config_, *region, targetSlot);
  const uint32_t nextSequence = currentInspect.validSlots == 0 ? 1 : currentInspect.latestSequence + 1;

  SlotHeader header;
  header.magic = kMagic;
  header.layoutVersion = config_.layoutVersion;
  header.recordType = region->recordType;
  header.schemaVersion = region->schemaVersion;
  header.flags = kFlagWriting;
  header.sequence = nextSequence;
  header.payloadLength = static_cast<uint16_t>(length);
  header.payloadCrc = crc32IsoHdlc(payload, length);
  header.headerCrc = headerCrcFor(header);

  uint8_t headerBytes[kHeaderSize];
  serializeHeader(header, headerBytes);
  if (!device_->write(targetAddress, headerBytes, kHeaderSize)) {
    return Result::WriteFailed;
  }
  if (length > 0 && !device_->write(targetAddress + kHeaderSize, payload, length)) {
    return Result::WriteFailed;
  }

  header.flags = kFlagValid;
  header.headerCrc = headerCrcFor(header);
  serializeHeader(header, headerBytes);
  if (!device_->write(targetAddress, headerBytes, kHeaderSize)) {
    return Result::WriteFailed;
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
  const RegionConfig* region = findRegion(recordType);
  SlotHeader bestHeader;
  uint16_t bestSlotIndex = 0;
  bool foundHeader = false;
  bool foundPayload = false;
  for (uint16_t slot = 0; slot < region->slotCount; ++slot) {
    SlotHeader header;
    if (!readHeader(*region, slot, header)) {
      return Result::DeviceOffline;
    }
    if (!validHeaderForRegion(*region, header)) {
      continue;
    }
    foundHeader = true;
    if (capacity < header.payloadLength) {
      return Result::PayloadTooLarge;
    }
    if (header.payloadLength > 0 &&
        !device_->read(slotAddress(config_, *region, slot) + kHeaderSize,
                       payload,
                       header.payloadLength)) {
      return Result::DeviceOffline;
    }
    if (crc32IsoHdlc(payload, header.payloadLength) != header.payloadCrc) {
      continue;
    }
    if (!foundPayload || sequenceAfter(header.sequence, bestHeader.sequence)) {
      bestHeader = header;
      bestSlotIndex = slot;
      foundPayload = true;
    }
  }
  if (!foundHeader) {
    return Result::FormatRequired;
  }
  if (!foundPayload) {
    return Result::CrcMismatch;
  }
  if (bestHeader.payloadLength > 0 &&
      !device_->read(slotAddress(config_, *region, bestSlotIndex) + kHeaderSize,
                     payload,
                     bestHeader.payloadLength)) {
    return Result::DeviceOffline;
  }
  if (crc32IsoHdlc(payload, bestHeader.payloadLength) != bestHeader.payloadCrc) {
    length = 0;
    return Result::CrcMismatch;
  }
  length = bestHeader.payloadLength;
  return Result::Ok;
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
  for (uint16_t slot = 0; slot < region->slotCount; ++slot) {
    SlotHeader header;
    if (!readHeader(*region, slot, header)) {
      out.lastResult = Result::DeviceOffline;
      return Result::DeviceOffline;
    }
    if (!validHeaderForRegion(*region, header)) {
      continue;
    }
    out.validSlots++;
    if (out.validSlots == 1 || sequenceAfter(header.sequence, out.latestSequence)) {
      out.latestSequence = header.sequence;
      out.latestSlotIndex = slot;
    }
  }
  out.estimatedWrites = out.latestSequence;
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

bool RecordStore::readHeader(const RegionConfig& region,
                             uint16_t slotIndex,
                             SlotHeader& out) const {
  uint8_t bytes[kHeaderSize];
  if (!device_->read(slotAddress(config_, region, slotIndex), bytes, kHeaderSize)) {
    return false;
  }
  out = deserializeHeader(bytes);
  return true;
}

bool RecordStore::validHeaderForRegion(const RegionConfig& region, const SlotHeader& header) const {
  if (header.magic != kMagic || header.flags != kFlagValid ||
      header.layoutVersion != config_.layoutVersion || header.recordType != region.recordType ||
      header.schemaVersion != region.schemaVersion ||
      header.payloadLength > region.slotSize - kHeaderSize) {
    return false;
  }
  if (headerCrcFor(header) != header.headerCrc) {
    return false;
  }
  return true;
}

bool RecordStore::readLatestHeader(const RegionConfig& region,
                                   SlotHeader& out,
                                   uint16_t& slotIndex) const {
  bool found = false;
  for (uint16_t slot = 0; slot < region.slotCount; ++slot) {
    SlotHeader header;
    if (!readHeader(region, slot, header)) {
      return false;
    }
    if (!validHeaderForRegion(region, header)) {
      continue;
    }
    if (!found || sequenceAfter(header.sequence, out.sequence)) {
      out = header;
      slotIndex = slot;
      found = true;
    }
  }
  return found;
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
