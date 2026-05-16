#include <cassert>
#include <cstddef>
#include <cstdint>

#include "DoorRecordFileStore.h"

namespace {

struct CaptureAppender {
  const char* path = nullptr;
  uint8_t bytes[kDoorRecordEncodedMaxBytes * 4] = {};
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
  DoorRecord record;
  record.sequence = 1;
  record.unixTime = 1800000010;
  record.uptimeSec = 20;
  record.bootId = 2;
  record.type = DoorRecordType::CommandRequested;
  record.result = DoorRecordResult::Ok;
  record.command = DoorCommand::Open;
  record.newTravelPulses = 10480;

  CaptureAppender capture;
  DoorRecordWriteResult result =
      appendDoorRecordToPath(record, "/records/door/current.dar", appendCapture, &capture);
  assert(result == DoorRecordWriteResult::Ok);
  assert(capture.path != nullptr);
  assert(capture.length == kDoorRecordEncodedMaxBytes);
  assert(verifyDoorEncodedRecord(capture.bytes, capture.length) == DoorRecordCodecResult::Ok);

  result = appendDoorRecordToPath(record, "", appendCapture, &capture);
  assert(result == DoorRecordWriteResult::InvalidArgument);

  capture.fail = true;
  result = appendDoorRecordToPath(record, "/records/door/current.dar", appendCapture, &capture);
  assert(result == DoorRecordWriteResult::WriteFailed);

  capture.fail = false;
  record.sequence = 2;
  record.unixTime = 1800003600;
  record.type = DoorRecordType::TravelSet;
  appendDoorRecordToPath(record, "/records/door/current.dar", appendCapture, &capture);
  record.sequence = 3;
  record.unixTime = 1800007200;
  record.type = DoorRecordType::FaultCleared;
  appendDoorRecordToPath(record, "/records/door/current.dar", appendCapture, &capture);

  DoorRecordPage page;
  assert(readDoorRecordPage("/records/door/current.dar",
                            0,
                            2,
                            fileSizeCapture,
                            readCapture,
                            &capture,
                            page) == DoorRecordReadResult::Ok);
  assert(page.totalRecords == 3);
  assert(page.startIndex == 0);
  assert(page.count == 2);
  assert(page.records[0].sequence == 1);
  assert(page.records[1].sequence == 2);
  assert(page.nextIndex == 2);

  DoorRecordQuery query;
  query.startIndex = 0;
  query.limit = 2;
  query.startUnixTime = 1800003000;
  query.typeFilterEnabled = true;
  query.type = DoorRecordType::FaultCleared;
  assert(readDoorRecordPage("/records/door/current.dar",
                            query,
                            fileSizeCapture,
                            readCapture,
                            &capture,
                            page) == DoorRecordReadResult::Ok);
  assert(page.totalRecords == 3);
  assert(page.count == 1);
  assert(page.records[0].sequence == 3);
  assert(page.nextIndex == 3);

  assert(readDoorRecordPage("/records/door/current.dar",
                            0,
                            0,
                            fileSizeCapture,
                            readCapture,
                            &capture,
                            page) == DoorRecordReadResult::InvalidArgument);

  return 0;
}
