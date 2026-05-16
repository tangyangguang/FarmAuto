#include "DoorRecordFileStore.h"

#include <cstdio>
#include <cstring>

namespace {

bool validPath(const char* path) {
  return path != nullptr && path[0] == '/';
}

bool archivePath(const char* currentPath, uint8_t archiveIndex, char* out, std::size_t outSize) {
  if (!validPath(currentPath) || archiveIndex == 0 || out == nullptr || outSize == 0) {
    return false;
  }
  const int written =
      std::snprintf(out, outSize, "%s.%u", currentPath, static_cast<unsigned>(archiveIndex));
  return written > 0 && static_cast<std::size_t>(written) < outSize;
}

}  // namespace

DoorRecordWriteResult appendDoorRecordToPath(const DoorRecord& record,
                                             const char* path,
                                             DoorRecordAppendBytes appendBytes,
                                             void* user) {
  if (!validPath(path) || appendBytes == nullptr) {
    return DoorRecordWriteResult::InvalidArgument;
  }

  uint8_t encoded[kDoorRecordEncodedMaxBytes];
  std::size_t encodedLength = 0;
  const DoorRecordEncodeResult encodeResult =
      encodeDoorRecord(record, encoded, sizeof(encoded), encodedLength);
  if (encodeResult.result != DoorRecordCodecResult::Ok ||
      encodedLength != encodeResult.bytesWritten) {
    return DoorRecordWriteResult::EncodeFailed;
  }

  return appendBytes(path, encoded, encodedLength, user) ? DoorRecordWriteResult::Ok
                                                        : DoorRecordWriteResult::WriteFailed;
}

DoorRecordRotateResult rotateDoorRecordPathIfNeeded(const char* currentPath,
                                                    uint32_t maxBytes,
                                                    uint8_t maxArchives,
                                                    std::size_t nextAppendBytes,
                                                    DoorRecordFileSize fileSize,
                                                    DoorRecordFileExists exists,
                                                    DoorRecordRemoveFile removeFile,
                                                    DoorRecordRenameFile renameFile,
                                                    void* user) {
  if (!validPath(currentPath) || maxBytes == 0 || maxArchives == 0 ||
      nextAppendBytes > maxBytes || fileSize == nullptr || exists == nullptr ||
      removeFile == nullptr || renameFile == nullptr) {
    return DoorRecordRotateResult::InvalidArgument;
  }
  if (!exists(currentPath, user)) {
    return DoorRecordRotateResult::Ok;
  }

  const int64_t size = fileSize(currentPath, user);
  if (size < 0) {
    return DoorRecordRotateResult::FileSizeFailed;
  }
  if (size + static_cast<int64_t>(nextAppendBytes) <= static_cast<int64_t>(maxBytes)) {
    return DoorRecordRotateResult::Ok;
  }

  char oldestPath[96];
  if (!archivePath(currentPath, maxArchives, oldestPath, sizeof(oldestPath))) {
    return DoorRecordRotateResult::InvalidArgument;
  }
  if (exists(oldestPath, user) && !removeFile(oldestPath, user)) {
    return DoorRecordRotateResult::RemoveFailed;
  }

  for (uint8_t index = maxArchives; index > 1; --index) {
    char fromPath[96];
    char toPath[96];
    if (!archivePath(currentPath, static_cast<uint8_t>(index - 1), fromPath, sizeof(fromPath)) ||
        !archivePath(currentPath, index, toPath, sizeof(toPath))) {
      return DoorRecordRotateResult::InvalidArgument;
    }
    if (exists(fromPath, user) && !renameFile(fromPath, toPath, user)) {
      return DoorRecordRotateResult::RenameFailed;
    }
  }

  char firstArchivePath[96];
  if (!archivePath(currentPath, 1, firstArchivePath, sizeof(firstArchivePath))) {
    return DoorRecordRotateResult::InvalidArgument;
  }
  return renameFile(currentPath, firstArchivePath, user) ? DoorRecordRotateResult::Ok
                                                        : DoorRecordRotateResult::RenameFailed;
}

DoorRecordReadResult readDoorRecordPage(const char* path,
                                        uint32_t startIndex,
                                        uint8_t limit,
                                        DoorRecordFileSize fileSize,
                                        DoorRecordReadBytesAt readBytesAt,
                                        void* user,
                                        DoorRecordPage& out) {
  DoorRecordQuery query;
  query.startIndex = startIndex;
  query.limit = limit;
  return readDoorRecordPage(path, query, fileSize, readBytesAt, user, out);
}

DoorRecordReadResult readDoorRecordPage(const char* path,
                                        const DoorRecordQuery& query,
                                        DoorRecordFileSize fileSize,
                                        DoorRecordReadBytesAt readBytesAt,
                                        void* user,
                                        DoorRecordPage& out) {
  out = DoorRecordPage{};
  out.startIndex = query.startIndex;
  out.nextIndex = query.startIndex;
  if (!validPath(path) || query.limit == 0 || fileSize == nullptr || readBytesAt == nullptr) {
    return DoorRecordReadResult::InvalidArgument;
  }

  const int64_t size = fileSize(path, user);
  if (size < 0) {
    return DoorRecordReadResult::NotReady;
  }
  out.totalRecords = static_cast<uint32_t>(size / static_cast<int64_t>(kDoorRecordEncodedMaxBytes));
  if (query.startIndex >= out.totalRecords) {
    out.nextIndex = out.totalRecords;
    return DoorRecordReadResult::Ok;
  }

  const uint8_t boundedLimit =
      query.limit > kDoorRecordPageMaxRecords ? kDoorRecordPageMaxRecords : query.limit;

  uint8_t buffer[kDoorRecordEncodedMaxBytes];
  uint32_t scanIndex = query.startIndex;
  while (scanIndex < out.totalRecords && out.count < boundedLimit) {
    const uint32_t offset = scanIndex * static_cast<uint32_t>(kDoorRecordEncodedMaxBytes);
    std::size_t readLength = 0;
    if (!readBytesAt(path, offset, buffer, sizeof(buffer), &readLength, user) ||
        readLength != sizeof(buffer)) {
      return DoorRecordReadResult::ReadFailed;
    }
    const DoorRecordCodecResult decodeResult =
        decodeDoorEncodedRecord(buffer, sizeof(buffer), out.records[out.count]);
    if (decodeResult == DoorRecordCodecResult::CrcMismatch) {
      return DoorRecordReadResult::CrcMismatch;
    }
    if (decodeResult != DoorRecordCodecResult::Ok) {
      return DoorRecordReadResult::ReadFailed;
    }
    ++scanIndex;

    const DoorRecord& decoded = out.records[out.count];
    if (query.startUnixTime > 0 && decoded.unixTime < query.startUnixTime) {
      continue;
    }
    if (query.endUnixTime > 0 && decoded.unixTime > query.endUnixTime) {
      continue;
    }
    if (query.typeFilterEnabled && decoded.type != query.type) {
      continue;
    }
    ++out.count;
  }
  out.nextIndex = scanIndex;

  return DoorRecordReadResult::Ok;
}
