#include <cassert>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "FeederPersistenceStore.h"
#include "FeederStorageLayout.h"

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
  FakeAt24cDevice device(kFeederAt24cConfig.totalBytes);
  Esp32At24cRecordStore::RecordStore store;
  assert(store.begin(device, kFeederAt24cConfig, kFeederAt24cRegions, kFeederAt24cRegionCount) ==
         Esp32At24cRecordStore::Result::Ok);

  FeederScheduleSnapshot schedule;
  schedule.serviceDate = 20260517;
  schedule.planCount = 1;
  schedule.plans[0].config.planId = 1;
  schedule.plans[0].config.enabled = true;
  schedule.plans[0].config.timeConfigured = true;
  schedule.plans[0].config.timeMinutes = 7 * 60 + 30;
  schedule.plans[0].config.channelMask = 0b0001;
  schedule.plans[0].config.targets[0].mode = FeederTargetMode::Grams;
  schedule.plans[0].config.targets[0].targetGramsX100 = 7000;
  schedule.plans[0].todayExecuted = true;

  assert(saveFeederSchedule(store, schedule) == Esp32At24cRecordStore::Result::Ok);
  FeederScheduleSnapshot restoredSchedule;
  assert(loadFeederSchedule(store, restoredSchedule) == Esp32At24cRecordStore::Result::Ok);
  assert(restoredSchedule.serviceDate == 20260517);
  assert(restoredSchedule.planCount == 1);
  assert(restoredSchedule.plans[0].config.planId == 1);
  assert(restoredSchedule.plans[0].todayExecuted);

  FeederTargetSnapshot targets;
  targets.channels[0].mode = FeederTargetMode::Grams;
  targets.channels[0].targetGramsX100 = 5000;
  targets.channels[1].mode = FeederTargetMode::Revolutions;
  targets.channels[1].targetRevolutionsX100 = 125;

  assert(saveFeederTargets(store, targets) == Esp32At24cRecordStore::Result::Ok);
  FeederTargetSnapshot restoredTargets;
  assert(loadFeederTargets(store, restoredTargets) == Esp32At24cRecordStore::Result::Ok);
  assert(restoredTargets.channels[0].mode == FeederTargetMode::Grams);
  assert(restoredTargets.channels[0].targetGramsX100 == 5000);
  assert(restoredTargets.channels[1].mode == FeederTargetMode::Revolutions);
  assert(restoredTargets.channels[1].targetRevolutionsX100 == 125);

  schedule.planCount = kFeederMaxPlans + 1;
  assert(saveFeederSchedule(store, schedule) == Esp32At24cRecordStore::Result::InvalidArgument);

  return 0;
}
