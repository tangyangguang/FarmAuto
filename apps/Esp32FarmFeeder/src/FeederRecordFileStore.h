#pragma once

#include <cstddef>
#include <cstdint>

#include "FeederRecordCodec.h"

enum class FeederRecordWriteResult : uint8_t {
  Ok,
  InvalidArgument,
  EncodeFailed,
  WriteFailed
};

enum class FeederRecordReadResult : uint8_t {
  Ok,
  InvalidArgument,
  NotReady,
  ReadFailed,
  CrcMismatch
};

static constexpr uint8_t kFeederRecordPageMaxRecords = 16;

struct FeederRecordPage {
  uint32_t totalRecords = 0;
  uint32_t startIndex = 0;
  uint32_t nextIndex = 0;
  uint8_t count = 0;
  FeederRecord records[kFeederRecordPageMaxRecords];
};

struct FeederRecordQuery {
  uint32_t startIndex = 0;
  uint8_t limit = kFeederRecordPageMaxRecords;
  uint32_t startUnixTime = 0;
  uint32_t endUnixTime = 0;
  bool typeFilterEnabled = false;
  FeederRecordType type = FeederRecordType::ManualRequested;
};

using FeederRecordAppendBytes =
    bool (*)(const char* path, const uint8_t* data, std::size_t length, void* user);

using FeederRecordFileSize = int64_t (*)(const char* path, void* user);

using FeederRecordReadBytesAt = bool (*)(const char* path,
                                         uint32_t offset,
                                         uint8_t* out,
                                         std::size_t maxLength,
                                         std::size_t* readLength,
                                         void* user);

FeederRecordWriteResult appendFeederRecordToPath(const FeederRecord& record,
                                                 const char* path,
                                                 FeederRecordAppendBytes appendBytes,
                                                 void* user);

FeederRecordReadResult readFeederRecordPage(const char* path,
                                            uint32_t startIndex,
                                            uint8_t limit,
                                            FeederRecordFileSize fileSize,
                                            FeederRecordReadBytesAt readBytesAt,
                                            void* user,
                                            FeederRecordPage& out);

FeederRecordReadResult readFeederRecordPage(const char* path,
                                            const FeederRecordQuery& query,
                                            FeederRecordFileSize fileSize,
                                            FeederRecordReadBytesAt readBytesAt,
                                            void* user,
                                            FeederRecordPage& out);
