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
  return result == Esp32At24cRecordStore::Result::Ok ? 0 : 1;
}
