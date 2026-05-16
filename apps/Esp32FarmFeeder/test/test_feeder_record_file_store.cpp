#include <cassert>
#include <cstddef>
#include <cstdint>

#include "FeederRecordFileStore.h"

namespace {

struct CaptureAppender {
  const char* path = nullptr;
  uint8_t bytes[kFeederRecordEncodedMaxBytes * 4] = {};
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

int64_t fileSizeCapture(const char*, void* user) {
  CaptureAppender* capture = static_cast<CaptureAppender*>(user);
  return capture == nullptr ? -1 : static_cast<int64_t>(capture->length);
}

bool readCapture(const char*,
                 uint32_t offset,
                 uint8_t* out,
                 std::size_t maxLength,
                 std::size_t* readLength,
                 void* user) {
  if (readLength) {
    *readLength = 0;
  }
  CaptureAppender* capture = static_cast<CaptureAppender*>(user);
  if (capture == nullptr || out == nullptr || offset > capture->length) {
    return false;
  }
  const std::size_t available = capture->length - offset;
  const std::size_t n = available < maxLength ? available : maxLength;
  for (std::size_t i = 0; i < n; ++i) {
    out[i] = capture->bytes[offset + i];
  }
  if (readLength) {
    *readLength = n;
  }
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

  capture.fail = false;
  appendFeederRecordToPath(record, "/records/feeder/current.far", appendCapture, &capture);
  record.sequence = 9;
  record.unixTime = 1800003600;
  record.type = FeederRecordType::ScheduleTriggered;
  appendFeederRecordToPath(record, "/records/feeder/current.far", appendCapture, &capture);

  FeederRecordPage page;
  assert(readFeederRecordPage("/records/feeder/current.far",
                              0,
                              2,
                              fileSizeCapture,
                              readCapture,
                              &capture,
                              page) == FeederRecordReadResult::Ok);
  assert(page.totalRecords == 3);
  assert(page.startIndex == 0);
  assert(page.count == 2);
  assert(page.records[0].sequence == 8);
  assert(page.records[1].sequence == 8);
  assert(page.nextIndex == 2);

  assert(readFeederRecordPage("/records/feeder/current.far",
                              1,
                              1,
                              fileSizeCapture,
                              readCapture,
                              &capture,
                              page) == FeederRecordReadResult::Ok);
  assert(page.totalRecords == 3);
  assert(page.startIndex == 1);
  assert(page.count == 1);
  assert(page.nextIndex == 2);

  FeederRecordQuery query;
  query.startIndex = 0;
  query.limit = 2;
  query.startUnixTime = 1800003000;
  query.typeFilterEnabled = true;
  query.type = FeederRecordType::ScheduleTriggered;
  assert(readFeederRecordPage("/records/feeder/current.far",
                              query,
                              fileSizeCapture,
                              readCapture,
                              &capture,
                              page) == FeederRecordReadResult::Ok);
  assert(page.totalRecords == 3);
  assert(page.startIndex == 0);
  assert(page.count == 1);
  assert(page.records[0].sequence == 9);
  assert(page.nextIndex == 3);

  assert(readFeederRecordPage("/records/feeder/current.far",
                              0,
                              0,
                              fileSizeCapture,
                              readCapture,
                              &capture,
                              page) == FeederRecordReadResult::InvalidArgument);

  return 0;
}
