#pragma once

#include <cstddef>
#include <cstdint>

#include "FeederTarget.h"

static constexpr uint32_t kFeederTargetMagic = 0xFA770001u;
static constexpr uint16_t kFeederTargetSchemaVersion = 1;
static constexpr std::size_t kFeederTargetHeaderBytes = 16;
static constexpr std::size_t kFeederTargetChannelBytes = 12;
static constexpr std::size_t kFeederTargetEncodedBytes =
    kFeederTargetHeaderBytes + (kFeederMaxChannels * kFeederTargetChannelBytes);

enum class FeederTargetCodecResult : uint8_t {
  Ok,
  InvalidArgument,
  BufferTooSmall,
  UnsupportedVersion,
  CrcMismatch
};

FeederTargetCodecResult encodeFeederTargetSnapshot(const FeederTargetSnapshot& snapshot,
                                                   uint8_t* out,
                                                   std::size_t capacity,
                                                   std::size_t& encodedLength);

FeederTargetCodecResult verifyFeederTargetSnapshot(const uint8_t* data, std::size_t length);

FeederTargetCodecResult decodeFeederTargetSnapshot(const uint8_t* data,
                                                   std::size_t length,
                                                   FeederTargetSnapshot& out);
