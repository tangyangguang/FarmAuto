#pragma once

#include <cstddef>
#include <cstdint>

#include "DoorRecordLog.h"

static constexpr uint32_t kDoorRecordMagic = 0xFAD00A01u;
static constexpr uint16_t kDoorRecordSchemaVersion = 1;
static constexpr std::size_t kDoorRecordHeaderSize = 32;
static constexpr std::size_t kDoorRecordPayloadSize = 44;
static constexpr std::size_t kDoorRecordEncodedMaxBytes =
    kDoorRecordHeaderSize + kDoorRecordPayloadSize;

enum class DoorRecordCodecResult : uint8_t {
  Ok,
  InvalidArgument,
  BufferTooSmall,
  CrcMismatch,
  UnsupportedVersion
};

struct DoorRecordEncodeResult {
  DoorRecordCodecResult result = DoorRecordCodecResult::InvalidArgument;
  std::size_t bytesWritten = 0;
  uint16_t payloadLength = 0;
  uint32_t headerCrc = 0;
  uint32_t payloadCrc = 0;
};

DoorRecordEncodeResult encodeDoorRecord(const DoorRecord& record,
                                        uint8_t* out,
                                        std::size_t capacity,
                                        std::size_t& encodedLength);

DoorRecordCodecResult verifyDoorEncodedRecord(const uint8_t* data, std::size_t length);

DoorRecordCodecResult decodeDoorEncodedRecord(const uint8_t* data,
                                              std::size_t length,
                                              DoorRecord& out);
