#pragma once

#include <cstddef>
#include <cstdint>

#include "DoorRecordCodec.h"

enum class DoorRecordWriteResult : uint8_t {
  Ok,
  InvalidArgument,
  EncodeFailed,
  WriteFailed
};

enum class DoorRecordReadResult : uint8_t {
  Ok,
  InvalidArgument,
  NotReady,
  ReadFailed,
  CrcMismatch
};

enum class DoorRecordRotateResult : uint8_t {
  Ok,
  InvalidArgument,
  FileSizeFailed,
  RemoveFailed,
  RenameFailed
};

static constexpr uint8_t kDoorRecordPageMaxRecords = 16;

struct DoorRecordPage {
  uint32_t totalRecords = 0;
  uint32_t startIndex = 0;
  uint32_t nextIndex = 0;
  uint8_t count = 0;
  DoorRecord records[kDoorRecordPageMaxRecords];
};

struct DoorRecordQuery {
  uint32_t startIndex = 0;
  uint8_t limit = kDoorRecordPageMaxRecords;
  uint32_t startUnixTime = 0;
  uint32_t endUnixTime = 0;
  bool typeFilterEnabled = false;
  DoorRecordType type = DoorRecordType::CommandRequested;
};

using DoorRecordAppendBytes =
    bool (*)(const char* path, const uint8_t* data, std::size_t length, void* user);

using DoorRecordFileSize = int64_t (*)(const char* path, void* user);

using DoorRecordReadBytesAt = bool (*)(const char* path,
                                       uint32_t offset,
                                       uint8_t* out,
                                       std::size_t maxLength,
                                       std::size_t* readLength,
                                       void* user);

using DoorRecordFileExists = bool (*)(const char* path, void* user);

using DoorRecordRemoveFile = bool (*)(const char* path, void* user);

using DoorRecordRenameFile = bool (*)(const char* from, const char* to, void* user);

DoorRecordWriteResult appendDoorRecordToPath(const DoorRecord& record,
                                             const char* path,
                                             DoorRecordAppendBytes appendBytes,
                                             void* user);

DoorRecordRotateResult rotateDoorRecordPathIfNeeded(const char* currentPath,
                                                    uint32_t maxBytes,
                                                    uint8_t maxArchives,
                                                    std::size_t nextAppendBytes,
                                                    DoorRecordFileSize fileSize,
                                                    DoorRecordFileExists exists,
                                                    DoorRecordRemoveFile removeFile,
                                                    DoorRecordRenameFile renameFile,
                                                    void* user);

DoorRecordReadResult readDoorRecordPage(const char* path,
                                        uint32_t startIndex,
                                        uint8_t limit,
                                        DoorRecordFileSize fileSize,
                                        DoorRecordReadBytesAt readBytesAt,
                                        void* user,
                                        DoorRecordPage& out);

DoorRecordReadResult readDoorRecordPage(const char* path,
                                        const DoorRecordQuery& query,
                                        DoorRecordFileSize fileSize,
                                        DoorRecordReadBytesAt readBytesAt,
                                        void* user,
                                        DoorRecordPage& out);
