#pragma once

#include <cstddef>
#include <cstdint>

#include "FeederSchedule.h"

static constexpr uint32_t kFeederScheduleMagic = 0xFA5C0ED1u;
static constexpr uint16_t kFeederScheduleSchemaVersion = 1;
static constexpr std::size_t kFeederScheduleHeaderBytes = 16;
static constexpr std::size_t kFeederSchedulePlanBytes = 46;
static constexpr std::size_t kFeederScheduleEncodedBytes =
    kFeederScheduleHeaderBytes + (kFeederMaxPlans * kFeederSchedulePlanBytes);

enum class FeederScheduleCodecResult : uint8_t {
  Ok,
  InvalidArgument,
  BufferTooSmall,
  UnsupportedVersion,
  CrcMismatch
};

FeederScheduleCodecResult encodeFeederScheduleSnapshot(const FeederScheduleSnapshot& snapshot,
                                                       uint8_t* out,
                                                       std::size_t capacity,
                                                       std::size_t& encodedLength);

FeederScheduleCodecResult verifyFeederScheduleSnapshot(const uint8_t* data, std::size_t length);

FeederScheduleCodecResult decodeFeederScheduleSnapshot(const uint8_t* data,
                                                       std::size_t length,
                                                       FeederScheduleSnapshot& out);
