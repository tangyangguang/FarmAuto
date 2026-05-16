#include <cassert>
#include <cstddef>
#include <cstdint>

#include "DoorStorageLayout.h"

int main() {
  assert(kDoorAt24cConfig.totalBytes == 16 * 1024);
  assert(kDoorAt24cConfig.pageSize == 64);
  assert(kDoorAt24cRegionCount == 7);

  uint32_t expectedOffset = 0;
  for (std::size_t i = 0; i < kDoorAt24cRegionCount; ++i) {
    const Esp32At24cRecordStore::RegionConfig& region = kDoorAt24cRegions[i];
    assert(region.offset == expectedOffset);
    assert(region.offset % kDoorAt24cConfig.pageSize == 0);
    assert(region.slotSize % kDoorAt24cConfig.pageSize == 0);
    expectedOffset += static_cast<uint32_t>(region.slotSize) * region.slotCount;
  }
  assert(expectedOffset == kDoorAt24cUsedBytes);
  assert(kDoorAt24cUsedBytes == 11776);
  assert(kDoorAt24cUsedBytes < kDoorAt24cConfig.totalBytes);
  assert(kDoorAt24cRegions[0].recordType ==
         static_cast<uint16_t>(DoorAt24cRecordType::RecoveryPolicy));
  assert(kDoorAt24cRegions[3].recordType ==
         static_cast<uint16_t>(DoorAt24cRecordType::MotionCheckpoint));

  return 0;
}
