#include <cassert>
#include <cstddef>
#include <cstdint>

#include "FeederStorageLayout.h"

int main() {
  assert(kFeederAt24cConfig.totalBytes == 16 * 1024);
  assert(kFeederAt24cConfig.pageSize == 64);
  assert(kFeederAt24cRegionCount == 8);

  uint32_t expectedOffset = 0;
  for (std::size_t i = 0; i < kFeederAt24cRegionCount; ++i) {
    const Esp32At24cRecordStore::RegionConfig& region = kFeederAt24cRegions[i];
    assert(region.offset == expectedOffset);
    assert(region.offset % kFeederAt24cConfig.pageSize == 0);
    assert(region.slotSize % kFeederAt24cConfig.pageSize == 0);
    expectedOffset += static_cast<uint32_t>(region.slotSize) * region.slotCount;
  }
  assert(expectedOffset == kFeederAt24cUsedBytes);
  assert(kFeederAt24cUsedBytes == 13824);
  assert(kFeederAt24cUsedBytes < kFeederAt24cConfig.totalBytes);
  assert(kFeederAt24cRegions[1].recordType ==
         static_cast<uint16_t>(FeederAt24cRecordType::Today));
  assert(kFeederAt24cRegions[2].slotSize >= kFeederScheduleEncodedBytes);
  assert(kFeederAt24cRegions[3].slotSize >= kFeederTargetEncodedBytes);
  assert(kFeederAt24cRegions[4].recordType ==
         static_cast<uint16_t>(FeederAt24cRecordType::BucketState));
  assert(kFeederAt24cRegions[4].slotSize >= kFeederBucketEncodedBytes);

  return 0;
}
