#pragma once

#include <cstddef>
#include <cstdint>

namespace FarmAutoEventLog {

enum Level : uint8_t {
  LEVEL_INFO = 1,
  LEVEL_WARN = 2,
  LEVEL_ERROR = 3
};

enum ValueMask : uint8_t {
  VALUE1 = 1 << 0,
  VALUE2 = 1 << 1,
  VALUE3 = 1 << 2
};

struct Event {
  Level level = LEVEL_INFO;
  char source[12] = {};
  char type[24] = {};
  char reason[24] = {};
  char object[56] = {};
  char text[32] = {};
  uint16_t code = 0;
  int32_t value1 = 0;
  int32_t value2 = 0;
  int32_t value3 = 0;
  uint8_t valueMask = 0;
};

struct BusinessEvent {
  uint32_t id = 0;
  uint32_t epochSec = 0;
  uint32_t bootId = 0;
  uint32_t uptimeSec = 0;
  bool timeSynced = false;
  Level level = LEVEL_INFO;
  char domain[24] = {};
  char action[32] = {};
  char target[56] = {};
  char message[96] = {};
  char detail[32] = {};
  uint16_t code = 0;
  int32_t value1 = 0;
  int32_t value2 = 0;
  int32_t value3 = 0;
  uint8_t valueMask = 0;
};

using ReadCallback = void (*)(const BusinessEvent& event, void* user);

bool recordDoorFaultCleared(const char* previousFault);
bool recordDoorProtectionStopped(const char* reason, int64_t positionPulses);

bool recordFeederFaultCleared(uint8_t channelMask);
bool recordFeederTodayCleared();
bool recordFeederBucketRefilled(uint8_t channelIndex, int32_t oldRemainGramsX100,
                                int32_t newRemainGramsX100);
bool recordFeederBucketRemainingSet(uint8_t channelIndex, int32_t oldRemainGramsX100,
                                    int32_t newRemainGramsX100);
bool recordScheduleSkipped(uint8_t planId, uint32_t serviceDate);
bool recordScheduleSkipCanceled(uint8_t planId, uint32_t serviceDate);
bool recordFeederBaseInfoChanged(uint8_t channelIndex);

bool recordStorageWarning(const char* medium, const char* operation, uint16_t code);

bool readLatest(uint16_t offset, uint16_t limit, ReadCallback callback, void* user = nullptr);
bool isReady();
bool faulted();
const char* lastError();
uint16_t count();
uint16_t capacity();

#if !defined(ARDUINO) || defined(FARMAUTO_EVENT_LOG_ENABLE_TEST_API)
using AppendSink = bool (*)(const Event& event, void* user);
void setAppendSinkForTest(AppendSink sink, void* user);
void resetAppendSinkForTest();
bool reservedSourceForTest(const char* source);
bool mapEventForTest(const Event& event, BusinessEvent& out);
#endif

}  // namespace FarmAutoEventLog
