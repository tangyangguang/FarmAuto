#include <cassert>
#include <cstdint>
#include <vector>

#include "Esp32At24cRecordStore.h"

class FakeAt24cDevice : public Esp32At24cRecordStore::IAt24cDevice {
public:
  explicit FakeAt24cDevice(std::size_t size) : bytes(size, 0xFF) {}

  bool read(uint32_t address, uint8_t* data, std::size_t length) override {
    if (address + length > bytes.size()) {
      return false;
    }
    for (std::size_t i = 0; i < length; ++i) {
      data[i] = bytes[address + i];
    }
    return true;
  }

  bool write(uint32_t address, const uint8_t* data, std::size_t length) override {
    if (address + length > bytes.size()) {
      return false;
    }
    for (std::size_t i = 0; i < length; ++i) {
      bytes[address + i] = data[i];
    }
    return true;
  }

  std::vector<uint8_t> bytes;
};

int main() {
  FakeAt24cDevice device(16 * 1024);
  Esp32At24cRecordStore::RecordStore store;
  Esp32At24cRecordStore::RecordStoreConfig config{};
  config.layoutVersion = 1;
  config.baseAddress = 0;
  config.totalBytes = 16 * 1024;

  Esp32At24cRecordStore::RegionConfig region{};
  region.recordType = 1;
  region.offset = 0;
  region.slotSize = 128;
  region.slotCount = 4;

  auto result = store.begin(device, config, &region, 1);
  assert(result == Esp32At24cRecordStore::Result::Ok);

  uint8_t firstPayload[] = {1, 2, 3, 4};
  assert(store.write(1, firstPayload, sizeof(firstPayload)) == Esp32At24cRecordStore::Result::Ok);

  uint8_t readBuffer[16] = {};
  std::size_t readLength = 0;
  assert(store.readLatest(1, readBuffer, sizeof(readBuffer), readLength) ==
         Esp32At24cRecordStore::Result::Ok);
  assert(readLength == sizeof(firstPayload));
  assert(readBuffer[0] == 1);
  assert(readBuffer[1] == 2);
  assert(readBuffer[2] == 3);
  assert(readBuffer[3] == 4);

  assert(store.write(1, firstPayload, sizeof(firstPayload)) ==
         Esp32At24cRecordStore::Result::Unchanged);

  uint8_t secondPayload[] = {5, 6, 7};
  assert(store.write(1, secondPayload, sizeof(secondPayload)) == Esp32At24cRecordStore::Result::Ok);

  Esp32At24cRecordStore::RecordInspect inspect;
  assert(store.inspect(1, inspect) == Esp32At24cRecordStore::Result::Ok);
  assert(inspect.validSlots == 2);
  assert(inspect.latestSequence == 2);

  readLength = 0;
  assert(store.readLatest(1, readBuffer, sizeof(readBuffer), readLength) ==
         Esp32At24cRecordStore::Result::Ok);
  assert(readLength == sizeof(secondPayload));
  assert(readBuffer[0] == 5);
  assert(readBuffer[1] == 6);
  assert(readBuffer[2] == 7);

  device.bytes[128 + 32] ^= 0xFF;
  assert(store.readLatest(1, readBuffer, sizeof(readBuffer), readLength) ==
         Esp32At24cRecordStore::Result::CrcMismatch);

  return 0;
}
