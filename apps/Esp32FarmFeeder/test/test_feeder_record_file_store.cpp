#include <cassert>
#include <cstddef>
#include <cstdint>

#include "FeederRecordFileStore.h"

namespace {

struct CaptureAppender {
  const char* path = nullptr;
  uint8_t bytes[kFeederRecordEncodedMaxBytes * 2] = {};
  std::size_t length = 0;
  bool fail = false;
};

bool appendCapture(const char* path, const uint8_t* data, std::size_t length, void* user) {
  CaptureAppender* capture = static_cast<CaptureAppender*>(user);
  if (capture == nullptr || capture->fail || path == nullptr || data == nullptr) {
    return false;
  }
  if (capture->length + length > sizeof(capture->bytes)) {
    return false;
  }
  capture->path = path;
  for (std::size_t i = 0; i < length; ++i) {
    capture->bytes[capture->length + i] = data[i];
  }
  capture->length += length;
  return true;
}

}  // namespace

int main() {
  FeederRecord record;
  record.sequence = 8;
  record.unixTime = 1800000010;
  record.uptimeSec = 20;
  record.bootId = 2;
  record.type = FeederRecordType::ManualRequested;
  record.result = FeederRecordResult::Ok;
  record.channel = 0;
  record.requestedMask = 0b0001;
  record.successMask = 0b0001;
  record.targetPulses = 4320;
  record.estimatedGramsX100 = 7000;
  record.actualPulses = 4300;

  CaptureAppender capture;
  FeederRecordWriteResult result =
      appendFeederRecordToPath(record, "/records/feeder/current.far", appendCapture, &capture);
  assert(result == FeederRecordWriteResult::Ok);
  assert(capture.path != nullptr);
  assert(capture.length == kFeederRecordEncodedMaxBytes);
  assert(verifyFeederEncodedRecord(capture.bytes, capture.length) == FeederRecordCodecResult::Ok);

  result = appendFeederRecordToPath(record, "", appendCapture, &capture);
  assert(result == FeederRecordWriteResult::InvalidArgument);

  capture.fail = true;
  result = appendFeederRecordToPath(record, "/records/feeder/current.far", appendCapture, &capture);
  assert(result == FeederRecordWriteResult::WriteFailed);

  return 0;
}
