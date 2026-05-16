#include <cassert>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "DoorRecoveryStore.h"
#include "DoorStorageLayout.h"

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
  FakeAt24cDevice device(kDoorAt24cConfig.totalBytes);
  Esp32At24cRecordStore::RecordStore store;
  assert(store.begin(device, kDoorAt24cConfig, kDoorAt24cRegions, kDoorAt24cRegionCount) ==
         Esp32At24cRecordStore::Result::Ok);

  DoorRecoveryState state;
  state.positionPulses = 100;
  state.openTargetPulses = 1000;
  state.closedPositionPulses = 0;
  state.maxRunPulses = 1500;
  state.maxRunMs = 25000;
  state.positionTrustLevel = PositionTrustLevel::Trusted;
  state.lastCommand = DoorCommand::Open;
  state.lastStopReason = DoorStopReason::TargetReached;
  state.unixTime = 1800000000;
  state.uptimeSec = 120;
  state.bootId = 3;

  assert(saveDoorRecoveryState(store, state) == Esp32At24cRecordStore::Result::Ok);

  DoorRecoveryState restored;
  assert(loadDoorRecoveryState(store, restored) == Esp32At24cRecordStore::Result::Ok);
  assert(restored.positionPulses == 100);
  assert(restored.openTargetPulses == 1000);
  assert(restored.maxRunMs == 25000);
  assert(restored.positionTrustLevel == PositionTrustLevel::Trusted);
  assert(restored.lastCommand == DoorCommand::Open);
  assert(restored.unixTime == 1800000000);

  state.openTargetPulses = 0;
  assert(saveDoorRecoveryState(store, state) == Esp32At24cRecordStore::Result::InvalidArgument);

  return 0;
}
