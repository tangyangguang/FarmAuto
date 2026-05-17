#pragma once

#include <cstddef>
#include <cstdint>

#include "FeederToday.h"

static constexpr uint32_t kFeederTodayMagic = 0xFEDA7001u;
static constexpr uint16_t kFeederTodaySchemaVersion = 1;
static constexpr std::size_t kFeederTodayHeaderBytes = 16;
static constexpr std::size_t kFeederTodayChannelBytes = 8;
static constexpr std::size_t kFeederTodayEncodedBytes =
    kFeederTodayHeaderBytes + 4 + (kFeederMaxChannels * kFeederTodayChannelBytes);

enum class FeederTodayCodecResult : uint8_t {
  Ok,
  InvalidArgument,
  BufferTooSmall,
  UnsupportedVersion,
  CrcMismatch
};

FeederTodayCodecResult encodeFeederTodaySnapshot(const FeederTodaySnapshot& snapshot,
                                                 uint8_t* out,
                                                 std::size_t capacity,
                                                 std::size_t& encodedLength);

FeederTodayCodecResult verifyFeederTodaySnapshot(const uint8_t* data, std::size_t length);

FeederTodayCodecResult decodeFeederTodaySnapshot(const uint8_t* data,
                                                 std::size_t length,
                                                 FeederTodaySnapshot& out);
