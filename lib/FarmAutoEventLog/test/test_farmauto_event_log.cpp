#include <cassert>
#include <cstring>

#include "FarmAutoEventLog.h"

namespace {

struct CapturedEvents {
  FarmAutoEventLog::Event events[8];
  uint8_t count = 0;
};

bool captureEvent(const FarmAutoEventLog::Event& event, void* user) {
  CapturedEvents* captured = static_cast<CapturedEvents*>(user);
  assert(captured != nullptr);
  assert(captured->count < 8);
  captured->events[captured->count++] = event;
  return true;
}

}  // namespace

int main() {
  assert(FarmAutoEventLog::reservedSourceForTest("wifi"));
  assert(FarmAutoEventLog::reservedSourceForTest("ota"));
  assert(FarmAutoEventLog::reservedSourceForTest("ntp"));
  assert(FarmAutoEventLog::reservedSourceForTest("fs"));
  assert(FarmAutoEventLog::reservedSourceForTest("boot"));
  assert(FarmAutoEventLog::reservedSourceForTest("esp32base"));
  assert(FarmAutoEventLog::reservedSourceForTest("system"));
  assert(!FarmAutoEventLog::reservedSourceForTest("farmdoor"));
  assert(!FarmAutoEventLog::reservedSourceForTest("farmfeeder"));
  assert(!FarmAutoEventLog::reservedSourceForTest("record_log"));

  CapturedEvents captured;
  FarmAutoEventLog::setAppendSinkForTest(captureEvent, &captured);

  assert(FarmAutoEventLog::recordDoorFaultCleared("PositionLost"));
  assert(captured.count == 1);
  assert(std::strcmp(captured.events[0].source, "farmdoor") == 0);
  assert(std::strcmp(captured.events[0].type, "maintenance") == 0);
  assert(std::strcmp(captured.events[0].reason, "fault_cleared") == 0);
  assert(std::strcmp(captured.events[0].object, "door") == 0);
  assert(std::strcmp(captured.events[0].text, "PositionLost") == 0);

  assert(FarmAutoEventLog::recordFeederBucketRefilled(1, 12000, 50000));
  assert(captured.count == 2);
  assert(std::strcmp(captured.events[1].source, "bucket") == 0);
  assert(std::strcmp(captured.events[1].type, "maintenance") == 0);
  assert(std::strcmp(captured.events[1].reason, "refilled") == 0);
  assert(std::strcmp(captured.events[1].object, "channel:2") == 0);
  assert(captured.events[1].value1 == 12000);
  assert(captured.events[1].value2 == 50000);
  assert(captured.events[1].valueMask == (FarmAutoEventLog::VALUE1 | FarmAutoEventLog::VALUE2));

  assert(FarmAutoEventLog::recordScheduleSkipped(3, 20260601));
  assert(captured.count == 3);
  assert(std::strcmp(captured.events[2].source, "schedule") == 0);
  assert(std::strcmp(captured.events[2].reason, "occurrence_skipped") == 0);
  assert(std::strcmp(captured.events[2].object, "plan:3") == 0);
  assert(captured.events[2].value1 == 20260601);

  assert(FarmAutoEventLog::recordStorageWarning("records", "append_failed", 17));
  assert(captured.count == 4);
  assert(std::strcmp(captured.events[3].source, "record_log") == 0);
  assert(std::strcmp(captured.events[3].type, "warning") == 0);
  assert(std::strcmp(captured.events[3].reason, "append_failed") == 0);
  assert(std::strcmp(captured.events[3].object, "records") == 0);
  assert(captured.events[3].code == 17);

  FarmAutoEventLog::BusinessEvent businessEvent;
  assert(FarmAutoEventLog::mapEventForTest(captured.events[0], businessEvent));
  assert(std::strcmp(businessEvent.domain, "farmdoor") == 0);
  assert(std::strcmp(businessEvent.action, "doorFaultCleared") == 0);
  assert(std::strcmp(businessEvent.target, "door") == 0);
  assert(std::strcmp(businessEvent.detail, "PositionLost") == 0);

  assert(FarmAutoEventLog::mapEventForTest(captured.events[1], businessEvent));
  assert(std::strcmp(businessEvent.domain, "bucket") == 0);
  assert(std::strcmp(businessEvent.action, "bucketRefilled") == 0);
  assert(businessEvent.value1 == 12000);
  assert(businessEvent.value2 == 50000);

  assert(FarmAutoEventLog::mapEventForTest(captured.events[3], businessEvent));
  assert(std::strcmp(businessEvent.domain, "recordLog") == 0);
  assert(std::strcmp(businessEvent.action, "storageWarning") == 0);
  assert(std::strcmp(businessEvent.detail, "append_failed") == 0);

  FarmAutoEventLog::resetAppendSinkForTest();

  return 0;
}
