#pragma once

#include <cstddef>
#include <cstdint>

#include "FeederBucket.h"

static constexpr uint32_t kFeederCalibrationMagic = 0xFACAA101u;
static constexpr uint16_t kFeederCalibrationSchemaVersion = 1;
static constexpr std::size_t kFeederCalibrationHeaderBytes = 16;
static constexpr std::size_t kFeederCalibrationChannelBytes = 16;
static constexpr std::size_t kFeederCalibrationEncodedBytes =
    kFeederCalibrationHeaderBytes + (kFeederMaxChannels * kFeederCalibrationChannelBytes);

enum class FeederCalibrationCodecResult : uint8_t {
  Ok,
  InvalidArgument,
  BufferTooSmall,
  UnsupportedVersion,
  CrcMismatch
};

FeederCalibrationCodecResult encodeFeederCalibrationSnapshot(const FeederBucketSnapshot& snapshot,
                                                             uint8_t* out,
                                                             std::size_t capacity,
                                                             std::size_t& encodedLength);

FeederCalibrationCodecResult verifyFeederCalibrationSnapshot(const uint8_t* data,
                                                             std::size_t length);

FeederCalibrationCodecResult decodeFeederCalibrationSnapshot(const uint8_t* data,
                                                             std::size_t length,
                                                             FeederBucketSnapshot& out);
