#include "DoorRecordFileStore.h"

namespace {

bool validPath(const char* path) {
  return path != nullptr && path[0] == '/';
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
