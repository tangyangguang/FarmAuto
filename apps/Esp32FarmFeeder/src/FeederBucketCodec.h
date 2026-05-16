#pragma once

#include <cstddef>
#include <cstdint>

#include "FeederBucket.h"

static constexpr uint32_t kFeederBucketMagic = 0xFA6B0C01u;
static constexpr uint16_t kFeederBucketSchemaVersion = 1;
static constexpr std::size_t kFeederBucketHeaderBytes = 16;
static constexpr std::size_t kFeederBucketChannelBytes = 24;
static constexpr std::size_t kFeederBucketEncodedBytes =
    kFeederBucketHeaderBytes + (kFeederMaxChannels * kFeederBucketChannelBytes);

enum class FeederBucketCodecResult : uint8_t {
  Ok,
  InvalidArgument,
  BufferTooSmall,
  UnsupportedVersion,
  CrcMismatch
};

FeederBucketCodecResult encodeFeederBucketSnapshot(const FeederBucketSnapshot& snapshot,
                                                   uint8_t* out,
                                                   std::size_t capacity,
                                                   std::size_t& encodedLength);

FeederBucketCodecResult verifyFeederBucketSnapshot(const uint8_t* data, std::size_t length);

FeederBucketCodecResult decodeFeederBucketSnapshot(const uint8_t* data,
                                                   std::size_t length,
                                                   FeederBucketSnapshot& out);
