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
