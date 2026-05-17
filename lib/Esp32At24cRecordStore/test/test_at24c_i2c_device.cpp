#include <cassert>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "Esp32At24cRecordStore.h"

class FakeI2cBus : public Esp32At24cRecordStore::II2cBus {
 public:
  struct WriteOp {
    uint8_t deviceAddress = 0;
    uint32_t memoryAddress = 0;
    std::size_t dataLength = 0;
  };

  explicit FakeI2cBus(std::size_t size) : bytes(size, 0xFF) {}

  bool write(uint8_t deviceAddress, const uint8_t* data, std::size_t length) override {
    if (length == 0) {
      ++ackPolls;
      return ackOnline;
    }
    if (length < 2) {
      return false;
    }
    const uint32_t address = (static_cast<uint32_t>(data[0]) << 8) | data[1];
    const std::size_t dataLength = length - 2;
    if (address + dataLength > bytes.size()) {
      return false;
    }
    for (std::size_t i = 0; i < dataLength; ++i) {
      bytes[address + i] = data[2 + i];
    }
    WriteOp op;
    op.deviceAddress = deviceAddress;
    op.memoryAddress = address;
    op.dataLength = dataLength;
    writes.push_back(op);
    return true;
  }

  bool writeRead(uint8_t,
                 const uint8_t* writeData,
                 std::size_t writeLength,
                 uint8_t* readData,
                 std::size_t readLength) override {
    if (writeLength != 2 || readData == nullptr) {
      return false;
    }
    const uint32_t address = (static_cast<uint32_t>(writeData[0]) << 8) | writeData[1];
    if (address + readLength > bytes.size()) {
      return false;
    }
    for (std::size_t i = 0; i < readLength; ++i) {
      readData[i] = bytes[address + i];
    }
    return true;
  }

  void delayMs(uint16_t ms) override {
    lastDelayMs = ms;
  }

  std::vector<uint8_t> bytes;
  std::vector<WriteOp> writes;
  uint16_t lastDelayMs = 0;
  uint8_t ackPolls = 0;
  bool ackOnline = true;
};

int main() {
  FakeI2cBus bus(256);
  Esp32At24cRecordStore::At24cI2cDeviceConfig config;
  config.deviceAddress = 0x50;
  config.totalBytes = 256;
  config.pageSize = 16;
  config.addressBytes = 2;
  config.maxTransferBytes = 8;
  config.writePollAttempts = 3;
  config.writePollDelayMs = 2;

  Esp32At24cRecordStore::At24cI2cDevice device(bus, config);
  uint8_t payload[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
  assert(device.write(14, payload, sizeof(payload)));
  assert(bus.writes.size() == 3);
  assert(bus.writes[0].memoryAddress == 14);
  assert(bus.writes[0].dataLength == 2);
  assert(bus.writes[1].memoryAddress == 16);
  assert(bus.writes[1].dataLength == 6);
  assert(bus.writes[2].memoryAddress == 22);
  assert(bus.writes[2].dataLength == 2);
  assert(bus.ackPolls == 3);
  assert(bus.lastDelayMs == 2);

  uint8_t readBuffer[10] = {};
  assert(device.read(14, readBuffer, sizeof(readBuffer)));
  for (std::size_t i = 0; i < sizeof(readBuffer); ++i) {
    assert(readBuffer[i] == payload[i]);
  }

  assert(!device.read(250, readBuffer, sizeof(readBuffer)));
  bus.ackOnline = false;
  assert(!device.write(40, payload, 1));

  config.maxTransferBytes = 2;
  Esp32At24cRecordStore::At24cI2cDevice invalid(bus, config);
  assert(!invalid.write(0, payload, 1));

  return 0;
}
