#include "FeederRecordFileStore.h"

namespace {

bool validPath(const char* path) {
  return path != nullptr && path[0] == '/';
}

}  // namespace

FeederRecordWriteResult appendFeederRecordToPath(const FeederRecord& record,
                                                 const char* path,
                                                 FeederRecordAppendBytes appendBytes,
                                                 void* user) {
  if (!validPath(path) || appendBytes == nullptr) {
    return FeederRecordWriteResult::InvalidArgument;
  }

  uint8_t encoded[kFeederRecordEncodedMaxBytes];
  std::size_t encodedLength = 0;
  const FeederRecordEncodeResult encodeResult =
      encodeFeederRecord(record, encoded, sizeof(encoded), encodedLength);
  if (encodeResult.result != FeederRecordCodecResult::Ok ||
      encodedLength != encodeResult.bytesWritten) {
    return FeederRecordWriteResult::EncodeFailed;
  }

  return appendBytes(path, encoded, encodedLength, user) ? FeederRecordWriteResult::Ok
                                                        : FeederRecordWriteResult::WriteFailed;
}

FeederRecordReadResult readFeederRecordPage(const char* path,
                                            uint32_t startIndex,
                                            uint8_t limit,
                                            FeederRecordFileSize fileSize,
                                            FeederRecordReadBytesAt readBytesAt,
                                            void* user,
                                            FeederRecordPage& out) {
  FeederRecordQuery query;
  query.startIndex = startIndex;
  query.limit = limit;
  return readFeederRecordPage(path, query, fileSize, readBytesAt, user, out);
}

FeederRecordReadResult readFeederRecordPage(const char* path,
                                            const FeederRecordQuery& query,
                                            FeederRecordFileSize fileSize,
                                            FeederRecordReadBytesAt readBytesAt,
                                            void* user,
                                            FeederRecordPage& out) {
  out = FeederRecordPage{};
  out.startIndex = query.startIndex;
  out.nextIndex = query.startIndex;
  if (!validPath(path) || query.limit == 0 || fileSize == nullptr || readBytesAt == nullptr) {
    return FeederRecordReadResult::InvalidArgument;
  }

  const int64_t size = fileSize(path, user);
  if (size < 0) {
    return FeederRecordReadResult::NotReady;
  }
  out.totalRecords = static_cast<uint32_t>(size / static_cast<int64_t>(kFeederRecordEncodedMaxBytes));
  if (query.startIndex >= out.totalRecords) {
    out.nextIndex = out.totalRecords;
    return FeederRecordReadResult::Ok;
  }

  const uint8_t boundedLimit =
      query.limit > kFeederRecordPageMaxRecords ? kFeederRecordPageMaxRecords : query.limit;

  uint8_t buffer[kFeederRecordEncodedMaxBytes];
  uint32_t scanIndex = query.startIndex;
  while (scanIndex < out.totalRecords && out.count < boundedLimit) {
    const uint32_t offset = scanIndex * static_cast<uint32_t>(kFeederRecordEncodedMaxBytes);
    std::size_t readLength = 0;
    if (!readBytesAt(path, offset, buffer, sizeof(buffer), &readLength, user) ||
        readLength != sizeof(buffer)) {
      return FeederRecordReadResult::ReadFailed;
    }
    const FeederRecordCodecResult decodeResult =
        decodeFeederEncodedRecord(buffer, sizeof(buffer), out.records[out.count]);
    if (decodeResult == FeederRecordCodecResult::CrcMismatch) {
      return FeederRecordReadResult::CrcMismatch;
    }
    if (decodeResult != FeederRecordCodecResult::Ok) {
      return FeederRecordReadResult::ReadFailed;
    }
    ++scanIndex;

    const FeederRecord& decoded = out.records[out.count];
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

  return FeederRecordReadResult::Ok;
}
