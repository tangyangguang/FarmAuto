#include "FarmAutoEventLog.h"

#include <cstdio>
#include <cstring>

#if defined(ARDUINO)
#include <Esp32Base.h>
#endif

namespace FarmAutoEventLog {
namespace {

#if !defined(ARDUINO) || defined(FARMAUTO_EVENT_LOG_ENABLE_TEST_API)
AppendSink g_testSink = nullptr;
void* g_testUser = nullptr;
#endif

void copySafe(char* dst, std::size_t len, const char* src) {
  if (dst == nullptr || len == 0) {
    return;
  }
  if (src == nullptr) {
    dst[0] = '\0';
    return;
  }
  std::snprintf(dst, len, "%s", src);
}

bool sameToken(const char* lhs, const char* rhs) {
  return lhs != nullptr && rhs != nullptr && std::strcmp(lhs, rhs) == 0;
}

bool reservedSource(const char* source) {
  return sameToken(source, "wifi") ||
         sameToken(source, "ota") ||
         sameToken(source, "ntp") ||
         sameToken(source, "fs") ||
         sameToken(source, "boot") ||
         sameToken(source, "esp32base") ||
         sameToken(source, "system");
}

bool makeEvent(Event& event,
               Level level,
               const char* source,
               const char* type,
               const char* reason,
               const char* object,
               const char* text = nullptr) {
  if (source == nullptr || source[0] == '\0' || reservedSource(source) ||
      type == nullptr || type[0] == '\0' ||
      reason == nullptr || reason[0] == '\0') {
    return false;
  }
  event = Event{};
  event.level = level;
  copySafe(event.source, sizeof(event.source), source);
  copySafe(event.type, sizeof(event.type), type);
  copySafe(event.reason, sizeof(event.reason), reason);
  copySafe(event.object, sizeof(event.object), object);
  copySafe(event.text, sizeof(event.text), text);
  return true;
}

bool append(const Event& event) {
#if !defined(ARDUINO) || defined(FARMAUTO_EVENT_LOG_ENABLE_TEST_API)
  if (g_testSink != nullptr) {
    return g_testSink(event, g_testUser);
  }
#endif
#if defined(ARDUINO) && defined(ESP32BASE_ENABLE_APP_EVENTS) && ESP32BASE_ENABLE_APP_EVENTS
  Esp32BaseAppEventLog::Event baseEvent;
  baseEvent.level = event.level == LEVEL_ERROR ? Esp32BaseAppEventLog::LEVEL_ERROR
                    : event.level == LEVEL_WARN ? Esp32BaseAppEventLog::LEVEL_WARN
                                                : Esp32BaseAppEventLog::LEVEL_INFO;
  baseEvent.source = event.source;
  baseEvent.type = event.type;
  baseEvent.reason = event.reason;
  baseEvent.object = event.object;
  baseEvent.code = event.code;
  baseEvent.value1 = event.value1;
  baseEvent.value2 = event.value2;
  baseEvent.value3 = event.value3;
  baseEvent.valueMask = event.valueMask;
  baseEvent.text = event.text;
  return Esp32BaseAppEventLog::append(baseEvent);
#else
  (void)event;
  return false;
#endif
}

bool appendWithValues(Event& event,
                      int32_t value1,
                      int32_t value2 = 0,
                      int32_t value3 = 0,
                      uint8_t valueMask = VALUE1) {
  event.value1 = value1;
  event.value2 = value2;
  event.value3 = value3;
  event.valueMask = valueMask;
  return append(event);
}

bool channelObject(uint8_t channelIndex, char* out, std::size_t len) {
  if (out == nullptr || len == 0 || channelIndex > 98) {
    return false;
  }
  return std::snprintf(out, len, "channel:%u", static_cast<unsigned>(channelIndex + 1)) > 0;
}

bool planObject(uint8_t planId, char* out, std::size_t len) {
  if (out == nullptr || len == 0) {
    return false;
  }
  return std::snprintf(out, len, "plan:%u", static_cast<unsigned>(planId)) > 0;
}

bool mapBusinessEvent(const Event& raw,
                      uint32_t id,
                      uint32_t epochSec,
                      uint32_t bootId,
                      uint32_t uptimeSec,
                      bool timeSynced,
                      BusinessEvent& out) {
  out = BusinessEvent{};
  out.id = id;
  out.epochSec = epochSec;
  out.bootId = bootId;
  out.uptimeSec = uptimeSec;
  out.timeSynced = timeSynced;
  out.level = raw.level;
  copySafe(out.target, sizeof(out.target), raw.object);
  copySafe(out.detail, sizeof(out.detail), raw.text[0] != '\0' ? raw.text : raw.reason);
  out.code = raw.code;
  out.value1 = raw.value1;
  out.value2 = raw.value2;
  out.value3 = raw.value3;
  out.valueMask = raw.valueMask;

  if (sameToken(raw.source, "farmdoor") && sameToken(raw.reason, "fault_cleared")) {
    copySafe(out.domain, sizeof(out.domain), "farmdoor");
    copySafe(out.action, sizeof(out.action), "doorFaultCleared");
    copySafe(out.message, sizeof(out.message), "自动门故障已清除");
    return true;
  }
  if (sameToken(raw.source, "door_motor") && sameToken(raw.reason, "protection_stopped")) {
    copySafe(out.domain, sizeof(out.domain), "doorMotor");
    copySafe(out.action, sizeof(out.action), "doorProtectionStopped");
    copySafe(out.message, sizeof(out.message), "自动门保护停机");
    return true;
  }
  if (sameToken(raw.source, "farmfeeder") && sameToken(raw.reason, "fault_cleared")) {
    copySafe(out.domain, sizeof(out.domain), "farmfeeder");
    copySafe(out.action, sizeof(out.action), "feederFaultCleared");
    copySafe(out.message, sizeof(out.message), "喂食器故障已清除");
    return true;
  }
  if (sameToken(raw.source, "farmfeeder") && sameToken(raw.reason, "today_cleared")) {
    copySafe(out.domain, sizeof(out.domain), "farmfeeder");
    copySafe(out.action, sizeof(out.action), "feederTodayCleared");
    copySafe(out.message, sizeof(out.message), "今日累计已清空");
    return true;
  }
  if (sameToken(raw.source, "bucket") && sameToken(raw.reason, "refilled")) {
    copySafe(out.domain, sizeof(out.domain), "bucket");
    copySafe(out.action, sizeof(out.action), "bucketRefilled");
    copySafe(out.message, sizeof(out.message), "料桶已补料");
    return true;
  }
  if (sameToken(raw.source, "bucket") && sameToken(raw.reason, "remaining_set")) {
    copySafe(out.domain, sizeof(out.domain), "bucket");
    copySafe(out.action, sizeof(out.action), "bucketRemainingSet");
    copySafe(out.message, sizeof(out.message), "料桶余量已设置");
    return true;
  }
  if (sameToken(raw.source, "schedule") && sameToken(raw.reason, "occurrence_skipped")) {
    copySafe(out.domain, sizeof(out.domain), "schedule");
    copySafe(out.action, sizeof(out.action), "scheduleSkipped");
    copySafe(out.message, sizeof(out.message), "计划实例已跳过");
    return true;
  }
  if (sameToken(raw.source, "schedule") && sameToken(raw.reason, "skip_canceled")) {
    copySafe(out.domain, sizeof(out.domain), "schedule");
    copySafe(out.action, sizeof(out.action), "scheduleSkipCanceled");
    copySafe(out.message, sizeof(out.message), "计划跳过已取消");
    return true;
  }
  if (sameToken(raw.source, "feeder_channel") && sameToken(raw.reason, "base_info_changed")) {
    copySafe(out.domain, sizeof(out.domain), "feederChannel");
    copySafe(out.action, sizeof(out.action), "feederBaseInfoChanged");
    copySafe(out.message, sizeof(out.message), "通道基础信息已修改");
    return true;
  }
  if (sameToken(raw.source, "record_log")) {
    copySafe(out.domain, sizeof(out.domain), "recordLog");
    copySafe(out.action, sizeof(out.action), "storageWarning");
    copySafe(out.message, sizeof(out.message), "业务记录存储告警");
    return true;
  }

  return false;
}

#if defined(ARDUINO) && defined(ESP32BASE_ENABLE_APP_EVENTS) && ESP32BASE_ENABLE_APP_EVENTS
Event eventFromBaseRecord(const Esp32BaseAppEventRecord& record) {
  Event event;
  event.level = record.level == Esp32BaseAppEventLog::LEVEL_ERROR ? LEVEL_ERROR
                : record.level == Esp32BaseAppEventLog::LEVEL_WARN ? LEVEL_WARN
                                                                    : LEVEL_INFO;
  copySafe(event.source, sizeof(event.source), record.source);
  copySafe(event.type, sizeof(event.type), record.type);
  copySafe(event.reason, sizeof(event.reason), record.reason);
  copySafe(event.object, sizeof(event.object), record.object);
  copySafe(event.text, sizeof(event.text), record.text);
  event.code = record.code;
  event.value1 = record.value1;
  event.value2 = record.value2;
  event.value3 = record.value3;
  event.valueMask = record.valueMask;
  return event;
}

struct ReadState {
  ReadCallback callback = nullptr;
  void* user = nullptr;
};

void readBaseEvent(const Esp32BaseAppEventRecord& record, void* user) {
  ReadState* state = static_cast<ReadState*>(user);
  if (state == nullptr || state->callback == nullptr) {
    return;
  }
  BusinessEvent event;
  if (!mapBusinessEvent(eventFromBaseRecord(record),
                        record.id,
                        record.epochSec,
                        record.bootId,
                        record.uptimeSec,
                        (record.flags & Esp32BaseAppEventLog::FLAG_TIME_SYNCED) != 0,
                        event)) {
    return;
  }
  state->callback(event, state->user);
}
#endif

}  // namespace

bool recordDoorFaultCleared(const char* previousFault) {
  Event event;
  return makeEvent(event, LEVEL_INFO, "farmdoor", "maintenance", "fault_cleared", "door",
                   previousFault) &&
         append(event);
}

bool recordDoorProtectionStopped(const char* reason, int64_t positionPulses) {
  Event event;
  const int64_t clamped = positionPulses > INT32_MAX ? INT32_MAX
                          : positionPulses < INT32_MIN ? INT32_MIN
                                                       : positionPulses;
  return makeEvent(event, LEVEL_WARN, "door_motor", "warning", "protection_stopped", "door",
                   reason) &&
         appendWithValues(event, static_cast<int32_t>(clamped));
}

bool recordFeederFaultCleared(uint8_t channelMask) {
  Event event;
  return makeEvent(event, LEVEL_INFO, "farmfeeder", "maintenance", "fault_cleared", "channels") &&
         appendWithValues(event, channelMask);
}

bool recordFeederTodayCleared() {
  Event event;
  return makeEvent(event, LEVEL_INFO, "farmfeeder", "maintenance", "today_cleared", "today") &&
         append(event);
}

bool recordFeederBucketRefilled(uint8_t channelIndex,
                                int32_t oldRemainGramsX100,
                                int32_t newRemainGramsX100) {
  char object[16] = {};
  Event event;
  return channelObject(channelIndex, object, sizeof(object)) &&
         makeEvent(event, LEVEL_INFO, "bucket", "maintenance", "refilled", object) &&
         appendWithValues(event,
                          oldRemainGramsX100,
                          newRemainGramsX100,
                          0,
                          VALUE1 | VALUE2);
}

bool recordFeederBucketRemainingSet(uint8_t channelIndex,
                                    int32_t oldRemainGramsX100,
                                    int32_t newRemainGramsX100) {
  char object[16] = {};
  Event event;
  return channelObject(channelIndex, object, sizeof(object)) &&
         makeEvent(event, LEVEL_INFO, "bucket", "maintenance", "remaining_set", object) &&
         appendWithValues(event,
                          oldRemainGramsX100,
                          newRemainGramsX100,
                          0,
                          VALUE1 | VALUE2);
}

bool recordScheduleSkipped(uint8_t planId, uint32_t serviceDate) {
  char object[16] = {};
  Event event;
  return planObject(planId, object, sizeof(object)) &&
         makeEvent(event, LEVEL_INFO, "schedule", "maintenance", "occurrence_skipped", object) &&
         appendWithValues(event, static_cast<int32_t>(serviceDate));
}

bool recordScheduleSkipCanceled(uint8_t planId, uint32_t serviceDate) {
  char object[16] = {};
  Event event;
  return planObject(planId, object, sizeof(object)) &&
         makeEvent(event, LEVEL_INFO, "schedule", "maintenance", "skip_canceled", object) &&
         appendWithValues(event, static_cast<int32_t>(serviceDate));
}

bool recordFeederBaseInfoChanged(uint8_t channelIndex) {
  char object[16] = {};
  Event event;
  return channelObject(channelIndex, object, sizeof(object)) &&
         makeEvent(event, LEVEL_INFO, "feeder_channel", "maintenance", "base_info_changed", object) &&
         append(event);
}

bool recordStorageWarning(const char* medium, const char* operation, uint16_t code) {
  Event event;
  if (!makeEvent(event, LEVEL_WARN, "record_log", "warning", operation, medium)) {
    return false;
  }
  event.code = code;
  return append(event);
}

bool readLatest(uint16_t offset, uint16_t limit, ReadCallback callback, void* user) {
  if (callback == nullptr) {
    return false;
  }
#if defined(ARDUINO) && defined(ESP32BASE_ENABLE_APP_EVENTS) && ESP32BASE_ENABLE_APP_EVENTS
  ReadState state;
  state.callback = callback;
  state.user = user;
  return Esp32BaseAppEventLog::readLatest(offset, limit, readBaseEvent, &state);
#else
  (void)offset;
  (void)limit;
  (void)user;
  return false;
#endif
}

bool isReady() {
#if defined(ARDUINO) && defined(ESP32BASE_ENABLE_APP_EVENTS) && ESP32BASE_ENABLE_APP_EVENTS
  return Esp32BaseAppEventLog::isReady();
#else
  return false;
#endif
}

bool faulted() {
#if defined(ARDUINO) && defined(ESP32BASE_ENABLE_APP_EVENTS) && ESP32BASE_ENABLE_APP_EVENTS
  return Esp32BaseAppEventLog::faulted();
#else
  return false;
#endif
}

const char* lastError() {
#if defined(ARDUINO) && defined(ESP32BASE_ENABLE_APP_EVENTS) && ESP32BASE_ENABLE_APP_EVENTS
  return Esp32BaseAppEventLog::lastError();
#else
  return "app_events_unavailable";
#endif
}

uint16_t count() {
#if defined(ARDUINO) && defined(ESP32BASE_ENABLE_APP_EVENTS) && ESP32BASE_ENABLE_APP_EVENTS
  return Esp32BaseAppEventLog::count();
#else
  return 0;
#endif
}

uint16_t capacity() {
#if defined(ARDUINO) && defined(ESP32BASE_ENABLE_APP_EVENTS) && ESP32BASE_ENABLE_APP_EVENTS
  return Esp32BaseAppEventLog::capacity();
#else
  return 0;
#endif
}

#if !defined(ARDUINO) || defined(FARMAUTO_EVENT_LOG_ENABLE_TEST_API)
void setAppendSinkForTest(AppendSink sink, void* user) {
  g_testSink = sink;
  g_testUser = user;
}

void resetAppendSinkForTest() {
  g_testSink = nullptr;
  g_testUser = nullptr;
}

bool reservedSourceForTest(const char* source) {
  return reservedSource(source);
}

bool mapEventForTest(const Event& event, BusinessEvent& out) {
  return mapBusinessEvent(event, 0, 0, 0, 0, false, out);
}
#endif

}  // namespace FarmAutoEventLog
