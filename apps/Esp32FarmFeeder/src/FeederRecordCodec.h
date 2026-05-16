#pragma once

#include <cstddef>
#include <cstdint>

#include "FeederRecordLog.h"

static constexpr uint32_t kFeederRecordMagic = 0xFAFEED01u;
static constexpr uint16_t kFeederRecordSchemaVersion = 1;
static constexpr std::size_t kFeederRecordHeaderSize = 32;
static constexpr std::size_t kFeederRecordPayloadSize = 28;
static constexpr std::size_t kFeederRecordEncodedMaxBytes =
    kFeederRecordHeaderSize + kFeederRecordPayloadSize;

enum class FeederRecordCodecResult : uint8_t {
  Ok,
  InvalidArgument,
  BufferTooSmall,
  CrcMismatch,
  UnsupportedVersion
};

struct FeederRecordEncodeResult {
  FeederRecordCodecResult result = FeederRecordCodecResult::InvalidArgument;
  std::size_t bytesWritten = 0;
  uint16_t payloadLength = 0;
  uint32_t headerCrc = 0;
  uint32_t payloadCrc = 0;
};

FeederRecordEncodeResult encodeFeederRecord(const FeederRecord& record,
                                            uint8_t* out,
                                            std::size_t capacity,
                                            std::size_t& encodedLength);

FeederRecordCodecResult verifyFeederEncodedRecord(const uint8_t* data, std::size_t length);
