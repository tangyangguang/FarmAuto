#include "FarmFeederApp.h"

#include <Arduino.h>
#include <Esp32Base.h>

#include <esp_system.h>
#include <Wire.h>

#include "FeederBucket.h"
#include "FeederBucketRestore.h"
#include "FeederConfirm.h"
#include "FeederController.h"
#include "FeederDate.h"
#include "FeederFeedSettlement.h"
#include "FeederPersistenceStore.h"
#include "FeederRecordFileStore.h"
#include "FeederRecordLog.h"
#include "FeederRunTracker.h"
#include "FeederSchedule.h"
#include "FeederStorageLayout.h"
#include "FeederTarget.h"
#include "FeederToday.h"

namespace {

FeederController g_feeder;
FeederControllerConfig g_feederConfig;
FeederScheduleService g_schedules;
FeederBucketService g_buckets;
FeederTargetService g_targets;
FeederTodayService g_today;
FeederRunTracker g_runs;
FeederRecordLog g_records;
FeederConfirmGuard g_confirm;
uint32_t g_nextCommandId = 1;
uint16_t g_lastScheduleMinute = 24 * 60;
uint32_t g_lastScheduleDate = 0;
bool g_recordStorageReady = false;
Esp32At24cRecordStore::ArduinoWireI2cBus g_at24cBus;
Esp32At24cRecordStore::At24cI2cDevice g_at24cDevice(g_at24cBus, {});
Esp32At24cRecordStore::RecordStore g_at24cStore;
bool g_at24cStoreReady = false;

static constexpr uint8_t kFarmFeederRouteCount = 29;
static constexpr uint8_t kFeederI2cSda = 21;
static constexpr uint8_t kFeederI2cScl = 22;
static constexpr const char* kFeederRecordRootDir = "/records";
static constexpr const char* kFeederRecordDir = "/records/feeder";
static constexpr const char* kFeederRecordCurrentPath = "/records/feeder/current.far";
static constexpr uint32_t kFeederRecordMaxCurrentBytes = 64UL * 1024UL;
static constexpr uint8_t kFeederRecordMaxArchives = 16;
static_assert(ESP32BASE_WEB_MAX_ROUTES >= kFarmFeederRouteCount,
              "Esp32FarmFeeder requires ESP32BASE_WEB_MAX_ROUTES >= 29");

uint8_t configuredChannelMask() {
  return static_cast<uint8_t>((1U << kFeederConfiguredChannels) - 1U);
}

bool validAppChannel(uint8_t channel) {
  return channel < kFeederConfiguredChannels;
}

void addFarmFeederApi(const char* path, Esp32BaseWeb::Handler handler) {
  if (!Esp32BaseWeb::addApi(path, handler)) {
    ESP32BASE_LOG_E("farmfeeder", "api_route_register_failed path=%s", path);
  }
}

uint32_t allocateCommandId() {
  const uint32_t commandId = g_nextCommandId;
  ++g_nextCommandId;
  if (g_nextCommandId == 0) {
    g_nextCommandId = 1;
  }
  return commandId;
}

void syncFeederConfigFromBaseInfo() {
  g_feederConfig.enabledChannelMask = static_cast<uint8_t>(
      g_buckets.enabledChannelMask() & g_feederConfig.installedChannelMask & configuredChannelMask());
  const FeederCommandResult result = g_feeder.configure(g_feederConfig);
  g_runs.stopAll();
  if (result != FeederCommandResult::Ok) {
    ESP32BASE_LOG_E("farmfeeder", "feeder_config_sync_failed result=%u",
                    static_cast<unsigned>(result));
  }
}

bool at24cWriteOk(Esp32At24cRecordStore::Result result) {
  return result == Esp32At24cRecordStore::Result::Ok ||
         result == Esp32At24cRecordStore::Result::Unchanged;
}

void persistFeederScheduleIfReady() {
  if (!g_at24cStoreReady) {
    return;
  }
  const Esp32At24cRecordStore::Result result =
      saveFeederSchedule(g_at24cStore, g_schedules.snapshot());
  if (!at24cWriteOk(result)) {
    ESP32BASE_LOG_W("farmfeeder", "schedule_save_failed result=%u",
                    static_cast<unsigned>(result));
  }
}

void persistFeederTodayIfReady() {
  if (!g_at24cStoreReady) {
    return;
  }
  const Esp32At24cRecordStore::Result result = saveFeederToday(g_at24cStore, g_today.snapshot());
  if (!at24cWriteOk(result)) {
    ESP32BASE_LOG_W("farmfeeder", "today_save_failed result=%u", static_cast<unsigned>(result));
  }
}

void persistFeederTargetsIfReady() {
  if (!g_at24cStoreReady) {
    return;
  }
  const Esp32At24cRecordStore::Result result =
      saveFeederTargets(g_at24cStore, g_targets.snapshot());
  if (!at24cWriteOk(result)) {
    ESP32BASE_LOG_W("farmfeeder", "targets_save_failed result=%u",
                    static_cast<unsigned>(result));
  }
}

void persistFeederBucketsIfReady() {
  if (!g_at24cStoreReady) {
    return;
  }
  const Esp32At24cRecordStore::Result result =
      saveFeederBuckets(g_at24cStore, g_buckets.snapshot());
  if (!at24cWriteOk(result)) {
    ESP32BASE_LOG_W("farmfeeder", "buckets_save_failed result=%u",
                    static_cast<unsigned>(result));
  }
}

void persistFeederCalibrationIfReady() {
  if (!g_at24cStoreReady) {
    return;
  }
  const Esp32At24cRecordStore::Result result =
      saveFeederCalibration(g_at24cStore, g_buckets.snapshot());
  if (!at24cWriteOk(result)) {
    ESP32BASE_LOG_W("farmfeeder", "calibration_save_failed result=%u",
                    static_cast<unsigned>(result));
  }
}

void initializeFeederAt24cStore() {
  const Esp32At24cRecordStore::Result beginResult =
      g_at24cStore.begin(g_at24cDevice,
                         kFeederAt24cConfig,
                         kFeederAt24cRegions,
                         kFeederAt24cRegionCount);
  if (beginResult != Esp32At24cRecordStore::Result::Ok) {
    ESP32BASE_LOG_W("farmfeeder", "at24c_store_begin_failed result=%u",
                    static_cast<unsigned>(beginResult));
    return;
  }
  g_at24cStoreReady = true;

  FeederScheduleSnapshot schedule;
  if (loadFeederSchedule(g_at24cStore, schedule) == Esp32At24cRecordStore::Result::Ok &&
      g_schedules.restore(schedule) == FeederScheduleResult::Ok) {
    g_lastScheduleDate = schedule.serviceDate;
  }

  FeederTodaySnapshot today;
  if (loadFeederToday(g_at24cStore, today) == Esp32At24cRecordStore::Result::Ok) {
    g_today.restore(today);
  }

  FeederTargetSnapshot targets;
  if (loadFeederTargets(g_at24cStore, targets) == Esp32At24cRecordStore::Result::Ok) {
    g_targets.restore(targets);
  }

  FeederBucketSnapshot calibration;
  FeederBucketSnapshot dynamicBuckets;
  const bool hasCalibration =
      loadFeederCalibration(g_at24cStore, calibration) == Esp32At24cRecordStore::Result::Ok;
  const bool hasDynamicBuckets =
      loadFeederBuckets(g_at24cStore, dynamicBuckets) == Esp32At24cRecordStore::Result::Ok;
  if (hasCalibration && hasDynamicBuckets) {
    restoreFeederBucketParts(g_buckets, calibration, dynamicBuckets);
  } else if (hasCalibration) {
    g_buckets.restore(calibration);
  }

  syncFeederConfigFromBaseInfo();
}

const char* deviceStateName(FeederDeviceState state) {
  switch (state) {
    case FeederDeviceState::Idle: return "Idle";
    case FeederDeviceState::Running: return "Running";
    case FeederDeviceState::Degraded: return "Degraded";
    case FeederDeviceState::Fault: return "Fault";
  }
  return "Unknown";
}

const char* channelStateName(FeederChannelState state) {
  switch (state) {
    case FeederChannelState::Disabled: return "Disabled";
    case FeederChannelState::Idle: return "Idle";
    case FeederChannelState::Running: return "Running";
    case FeederChannelState::Completed: return "Completed";
    case FeederChannelState::Fault: return "Fault";
  }
  return "Unknown";
}

const char* runSourceName(FeederRunSource source) {
  switch (source) {
    case FeederRunSource::Manual: return "Manual";
    case FeederRunSource::Schedule: return "Schedule";
  }
  return "Manual";
}

const char* recordTypeName(FeederRecordType type) {
  switch (type) {
    case FeederRecordType::ManualRequested: return "FeederManualRequested";
    case FeederRecordType::ScheduleTriggered: return "FeederScheduleTriggered";
    case FeederRecordType::ScheduleMissed: return "FeederScheduleMissed";
    case FeederRecordType::ChannelStarted: return "FeederChannelStarted";
    case FeederRecordType::ChannelStopped: return "FeederChannelStopped";
    case FeederRecordType::BatchCompleted: return "FeederBatchCompleted";
    case FeederRecordType::FaultCleared: return "FeederFaultCleared";
    case FeederRecordType::TodayCleared: return "FeederTodayCleared";
  }
  return "UnknownEvent";
}

bool recordTypeFromName(const char* name, FeederRecordType& out) {
  if (name == nullptr || name[0] == '\0') {
    return false;
  }
  if (strcmp(name, "FeederManualRequested") == 0 || strcmp(name, "ManualRequested") == 0) {
    out = FeederRecordType::ManualRequested;
    return true;
  }
  if (strcmp(name, "FeederScheduleTriggered") == 0 || strcmp(name, "ScheduleTriggered") == 0) {
    out = FeederRecordType::ScheduleTriggered;
    return true;
  }
  if (strcmp(name, "FeederScheduleMissed") == 0 || strcmp(name, "ScheduleMissed") == 0) {
    out = FeederRecordType::ScheduleMissed;
    return true;
  }
  if (strcmp(name, "FeederChannelStarted") == 0 || strcmp(name, "ChannelStarted") == 0) {
    out = FeederRecordType::ChannelStarted;
    return true;
  }
  if (strcmp(name, "FeederChannelStopped") == 0 || strcmp(name, "ChannelStopped") == 0) {
    out = FeederRecordType::ChannelStopped;
    return true;
  }
  if (strcmp(name, "FeederBatchCompleted") == 0 || strcmp(name, "BatchCompleted") == 0) {
    out = FeederRecordType::BatchCompleted;
    return true;
  }
  if (strcmp(name, "FeederFaultCleared") == 0 || strcmp(name, "FaultCleared") == 0) {
    out = FeederRecordType::FaultCleared;
    return true;
  }
  if (strcmp(name, "FeederTodayCleared") == 0 || strcmp(name, "TodayCleared") == 0) {
    out = FeederRecordType::TodayCleared;
    return true;
  }
  return false;
}

const char* recordResultName(FeederRecordResult result) {
  switch (result) {
    case FeederRecordResult::Ok: return "Ok";
    case FeederRecordResult::Partial: return "Partial";
    case FeederRecordResult::Busy: return "Busy";
    case FeederRecordResult::Fault: return "Fault";
    case FeederRecordResult::Skipped: return "Skipped";
    case FeederRecordResult::InvalidArgument: return "InvalidArgument";
  }
  return "InvalidArgument";
}

void sendUint8(uint8_t value) {
  char number[8];
  snprintf(number, sizeof(number), "%u", static_cast<unsigned>(value));
  Esp32BaseWeb::sendChunk(number);
}

void sendInt32(int32_t value) {
  char number[16];
  snprintf(number, sizeof(number), "%ld", static_cast<long>(value));
  Esp32BaseWeb::sendChunk(number);
}

void sendUint32(uint32_t value) {
  char number[16];
  snprintf(number, sizeof(number), "%lu", static_cast<unsigned long>(value));
  Esp32BaseWeb::sendChunk(number);
}

FeederRecordTime currentRecordTime() {
  FeederRecordTime time;
#if ESP32BASE_ENABLE_NTP
  const Esp32BaseNtp::TimeSnapshot snapshot = Esp32BaseNtp::snapshot();
  time.unixTime = snapshot.synced ? snapshot.epochSec : 0;
  time.uptimeSec = snapshot.uptimeSec;
  time.bootId = snapshot.bootId;
#endif
  return time;
}

#if ESP32BASE_ENABLE_FS
bool appendFeederRecordBytes(const char* path, const uint8_t* data, std::size_t length, void*) {
  return Esp32BaseFs::appendBytes(path, data, length);
}

bool feederRecordPathExists(const char* path, void*) {
  return Esp32BaseFs::exists(path);
}

int64_t feederRecordFileSize(const char* path, void*) {
  return Esp32BaseFs::fileSize(path);
}

bool removeFeederRecordFile(const char* path, void*) {
  return Esp32BaseFs::removeFile(path);
}

bool renameFeederRecordFile(const char* from, const char* to, void*) {
  return Esp32BaseFs::rename(from, to);
}

bool readFeederRecordBytesAt(const char* path,
                             uint32_t offset,
                             uint8_t* out,
                             std::size_t maxLength,
                             std::size_t* readLength,
                             void*) {
  return Esp32BaseFs::readBytesAt(path, offset, out, maxLength, readLength);
}

bool ensureRecordStorageReady() {
  if (g_recordStorageReady) {
    return true;
  }
  if (!Esp32BaseFs::isReady()) {
    return false;
  }
  if (!Esp32BaseFs::exists(kFeederRecordRootDir) && !Esp32BaseFs::mkdir(kFeederRecordRootDir)) {
    return false;
  }
  if (!Esp32BaseFs::exists(kFeederRecordDir) && !Esp32BaseFs::mkdir(kFeederRecordDir)) {
    return false;
  }
  g_recordStorageReady = true;
  return true;
}
#endif

bool feederRecordPathForArchive(uint32_t archiveIndex, char* out, std::size_t outSize) {
  if (out == nullptr || outSize == 0 || archiveIndex > kFeederRecordMaxArchives) {
    return false;
  }
  const int written = archiveIndex == 0
                        ? snprintf(out, outSize, "%s", kFeederRecordCurrentPath)
                        : snprintf(out,
                                   outSize,
                                   "%s.%lu",
                                   kFeederRecordCurrentPath,
                                   static_cast<unsigned long>(archiveIndex));
  return written > 0 && static_cast<std::size_t>(written) < outSize;
}

void recordBusinessEvent(const FeederRecord& record) {
  const FeederRecord stored = g_records.append(record, currentRecordTime());
#if ESP32BASE_ENABLE_FS
  if (!ensureRecordStorageReady()) {
    return;
  }
  const FeederRecordRotateResult rotateResult =
      rotateFeederRecordPathIfNeeded(kFeederRecordCurrentPath,
                                     kFeederRecordMaxCurrentBytes,
                                     kFeederRecordMaxArchives,
                                     kFeederRecordEncodedMaxBytes,
                                     feederRecordFileSize,
                                     feederRecordPathExists,
                                     removeFeederRecordFile,
                                     renameFeederRecordFile,
                                     nullptr);
  if (rotateResult != FeederRecordRotateResult::Ok) {
    ESP32BASE_LOG_W("farmfeeder", "record_rotate_failed result=%u",
                    static_cast<unsigned>(rotateResult));
  }
  const FeederRecordWriteResult result = appendFeederRecordToPath(
      stored, kFeederRecordCurrentPath, appendFeederRecordBytes, nullptr);
  if (result != FeederRecordWriteResult::Ok) {
    ESP32BASE_LOG_W("farmfeeder", "record_append_failed result=%u",
                    static_cast<unsigned>(result));
  }
#endif
}

void sendChannelArray(const FeederSnapshot& snapshot, const FeederRunSnapshot& runs) {
  Esp32BaseWeb::sendChunk("[");
  for (uint8_t i = 0; i < kFeederConfiguredChannels; ++i) {
    if (i > 0) {
      Esp32BaseWeb::sendChunk(",");
    }
    Esp32BaseWeb::sendChunk("{\"index\":");
    sendUint8(i);
    Esp32BaseWeb::sendChunk(",\"state\":\"");
    Esp32BaseWeb::sendChunk(channelStateName(snapshot.channels[i]));
    Esp32BaseWeb::sendChunk("\",\"run\":{\"active\":");
    const FeederRunChannel& run = runs.channels[i];
    Esp32BaseWeb::sendChunk(run.active ? "true" : "false");
    Esp32BaseWeb::sendChunk(",\"source\":\"");
    Esp32BaseWeb::sendChunk(runSourceName(run.source));
    Esp32BaseWeb::sendChunk("\",\"targetPulses\":");
    char number[16];
    snprintf(number, sizeof(number), "%ld", static_cast<long>(run.targetPulses));
    Esp32BaseWeb::sendChunk(number);
    Esp32BaseWeb::sendChunk(",\"estimatedGramsX100\":");
    snprintf(number, sizeof(number), "%ld", static_cast<long>(run.estimatedGramsX100));
    Esp32BaseWeb::sendChunk(number);
    Esp32BaseWeb::sendChunk(",\"actualPulses\":");
    snprintf(number, sizeof(number), "%ld", static_cast<long>(run.actualPulses));
    Esp32BaseWeb::sendChunk(number);
    Esp32BaseWeb::sendChunk("}}");
  }
  Esp32BaseWeb::sendChunk("]");
}

void sendScheduleSummary(const FeederScheduleSnapshot& snapshot) {
  Esp32BaseWeb::sendChunk("{\"maxPlans\":");
  sendUint8(kFeederMaxPlans);
  Esp32BaseWeb::sendChunk(",\"planCount\":");
  sendUint8(snapshot.planCount);
  Esp32BaseWeb::sendChunk(",\"timeValid\":");
#if ESP32BASE_ENABLE_NTP
  const Esp32BaseNtp::TimeSnapshot timeSnapshot = Esp32BaseNtp::snapshot();
  Esp32BaseWeb::sendChunk(timeSnapshot.synced ? "true" : "false");
#else
  Esp32BaseWeb::sendChunk("false");
#endif
  Esp32BaseWeb::sendChunk(",\"serviceDate\":");
  char summaryNumber[16];
  snprintf(summaryNumber, sizeof(summaryNumber), "%lu", static_cast<unsigned long>(snapshot.serviceDate));
  Esp32BaseWeb::sendChunk(summaryNumber);
  Esp32BaseWeb::sendChunk(",\"lastEvaluatedMinute\":");
  snprintf(summaryNumber, sizeof(summaryNumber), "%u", static_cast<unsigned>(g_lastScheduleMinute));
  Esp32BaseWeb::sendChunk(summaryNumber);
  Esp32BaseWeb::sendChunk(",\"plans\":[");
  for (uint8_t i = 0; i < snapshot.planCount; ++i) {
    if (i > 0) {
      Esp32BaseWeb::sendChunk(",");
    }
    const FeederPlanState& plan = snapshot.plans[i];
    Esp32BaseWeb::sendChunk("{\"planId\":");
    sendUint8(plan.config.planId);
    Esp32BaseWeb::sendChunk(",\"enabled\":");
    Esp32BaseWeb::sendChunk(plan.config.enabled ? "true" : "false");
    Esp32BaseWeb::sendChunk(",\"timeMinutes\":");
    char number[8];
    snprintf(number, sizeof(number), "%u", static_cast<unsigned>(plan.config.timeMinutes));
    Esp32BaseWeb::sendChunk(number);
    Esp32BaseWeb::sendChunk(",\"channelMask\":");
    sendUint8(plan.config.channelMask);
    Esp32BaseWeb::sendChunk(",\"skipToday\":");
    Esp32BaseWeb::sendChunk(plan.skipToday ? "true" : "false");
    Esp32BaseWeb::sendChunk(",\"skipServiceDate\":");
    sendUint32(plan.skipServiceDate);
    Esp32BaseWeb::sendChunk(",\"scheduleAttemptedToday\":");
    Esp32BaseWeb::sendChunk(plan.scheduleAttemptedToday ? "true" : "false");
    Esp32BaseWeb::sendChunk(",\"todayExecuted\":");
    Esp32BaseWeb::sendChunk(plan.todayExecuted ? "true" : "false");
    Esp32BaseWeb::sendChunk(",\"scheduleMissedToday\":");
    Esp32BaseWeb::sendChunk(plan.scheduleMissedToday ? "true" : "false");
    Esp32BaseWeb::sendChunk("}");
  }
  Esp32BaseWeb::sendChunk("]}");
}

void sendPlanTime(uint16_t timeMinutes) {
  sendUint8(static_cast<uint8_t>(timeMinutes / 60));
  Esp32BaseWeb::sendChunk(":");
  if (timeMinutes % 60 < 10) {
    Esp32BaseWeb::sendChunk("0");
  }
  sendUint8(static_cast<uint8_t>(timeMinutes % 60));
}

void sendPlanTargetSummary(const FeederPlanConfig& config) {
  bool first = true;
  for (uint8_t i = 0; i < kFeederConfiguredChannels; ++i) {
    const uint8_t bit = static_cast<uint8_t>(1U << i);
    if ((config.channelMask & bit) == 0) {
      continue;
    }
    if (!first) {
      Esp32BaseWeb::sendChunk("；");
    }
    first = false;
    Esp32BaseWeb::sendChunk("通道");
    sendUint8(static_cast<uint8_t>(i + 1));
    Esp32BaseWeb::sendChunk(" ");
    const FeederChannelTarget& target = config.targets[i];
    if (target.mode == FeederTargetMode::Grams) {
      sendInt32(target.targetGramsX100);
      Esp32BaseWeb::sendChunk("/100 g");
    } else if (target.mode == FeederTargetMode::Revolutions) {
      sendInt32(target.targetRevolutionsX100);
      Esp32BaseWeb::sendChunk("/100 圈");
    } else {
      Esp32BaseWeb::sendChunk("未配置");
    }
  }
  if (first) {
    Esp32BaseWeb::sendChunk("未配置通道");
  }
}

void sendOccurrenceStatus(const FeederPlanState& plan,
                          uint32_t serviceDate,
                          uint32_t currentServiceDate) {
  if (!plan.config.enabled || !plan.config.timeConfigured) {
    Esp32BaseWeb::sendChunk("未启用");
    return;
  }
  if (plan.skipServiceDate == serviceDate) {
    Esp32BaseWeb::sendChunk("已跳过本次");
    return;
  }
  if (serviceDate == currentServiceDate) {
    if (plan.todayExecuted) {
      Esp32BaseWeb::sendChunk("已完成");
    } else if (plan.scheduleAttemptedToday) {
      Esp32BaseWeb::sendChunk("已尝试");
    } else if (plan.scheduleMissedToday) {
      Esp32BaseWeb::sendChunk("已错过");
    } else {
      Esp32BaseWeb::sendChunk("待执行");
    }
    return;
  }
  Esp32BaseWeb::sendChunk("将执行");
}

void sendOccurrenceAction(const FeederPlanState& plan, uint32_t serviceDate) {
  if (!plan.config.enabled || !plan.config.timeConfigured || serviceDate == 0) {
    Esp32BaseWeb::sendChunk("-");
    return;
  }
  const bool skipped = plan.skipServiceDate == serviceDate;
  Esp32BaseWeb::sendChunk("<form method='post' action='");
  Esp32BaseWeb::sendChunk(skipped ? "/api/app/schedule-occurrence/cancel-skip"
                                  : "/api/app/schedule-occurrence/skip");
  Esp32BaseWeb::sendChunk("'><input type='hidden' name='planId' value='");
  sendUint8(plan.config.planId);
  Esp32BaseWeb::sendChunk("'><input type='hidden' name='date' value='");
  sendUint32(serviceDate);
  Esp32BaseWeb::sendChunk("'><button>");
  Esp32BaseWeb::sendChunk(skipped ? "取消跳过" : "跳过本次");
  Esp32BaseWeb::sendChunk("</button></form>");
}

void sendOccurrenceTable(const FeederScheduleSnapshot& schedules,
                         uint32_t serviceDate,
                         const char* title) {
  Esp32BaseWeb::sendChunk("<section><h2>");
  Esp32BaseWeb::sendChunk(title);
  Esp32BaseWeb::sendChunk("</h2>");
  if (serviceDate == 0) {
    Esp32BaseWeb::sendChunk("<p>时间未同步，暂不能展示具体执行日期。</p></section>");
    return;
  }
  Esp32BaseWeb::sendChunk("<p>服务日期 ");
  sendUint32(serviceDate);
  Esp32BaseWeb::sendChunk("</p><table><tr><th>时间</th><th>计划</th><th>状态</th><th>目标</th><th>操作</th></tr>");
  for (uint8_t i = 0; i < schedules.planCount; ++i) {
    const FeederPlanState& plan = schedules.plans[i];
    Esp32BaseWeb::sendChunk("<tr><td>");
    sendPlanTime(plan.config.timeMinutes);
    Esp32BaseWeb::sendChunk("</td><td>计划 ");
    sendUint8(plan.config.planId);
    Esp32BaseWeb::sendChunk("</td><td>");
    sendOccurrenceStatus(plan, serviceDate, schedules.serviceDate);
    Esp32BaseWeb::sendChunk("</td><td>");
    sendPlanTargetSummary(plan.config);
    Esp32BaseWeb::sendChunk("</td><td>");
    sendOccurrenceAction(plan, serviceDate);
    Esp32BaseWeb::sendChunk("</td></tr>");
  }
  Esp32BaseWeb::sendChunk("</table></section>");
}

void sendBucketSummary(const FeederBucketSnapshot& snapshot) {
  Esp32BaseWeb::sendChunk("[");
  for (uint8_t i = 0; i < kFeederConfiguredChannels; ++i) {
    if (i > 0) {
      Esp32BaseWeb::sendChunk(",");
    }
    const FeederBucketState& channel = snapshot.channels[i];
    Esp32BaseWeb::sendChunk("{\"channel\":");
    sendUint8(i);
    Esp32BaseWeb::sendChunk(",\"enabled\":");
    Esp32BaseWeb::sendChunk(channel.baseInfo.enabled ? "true" : "false");
    Esp32BaseWeb::sendChunk(",\"outputPulsesPerRev\":");
    char number[16];
    snprintf(number, sizeof(number), "%ld", static_cast<long>(channel.baseInfo.outputPulsesPerRev));
    Esp32BaseWeb::sendChunk(number);
    Esp32BaseWeb::sendChunk(",\"gramsPerRevX100\":");
    snprintf(number, sizeof(number), "%ld", static_cast<long>(channel.baseInfo.gramsPerRevX100));
    Esp32BaseWeb::sendChunk(number);
    Esp32BaseWeb::sendChunk(",\"capacityGramsX100\":");
    snprintf(number, sizeof(number), "%ld", static_cast<long>(channel.baseInfo.capacityGramsX100));
    Esp32BaseWeb::sendChunk(number);
    Esp32BaseWeb::sendChunk(",\"remainGramsX100\":");
    snprintf(number, sizeof(number), "%ld", static_cast<long>(channel.remainGramsX100));
    Esp32BaseWeb::sendChunk(number);
    Esp32BaseWeb::sendChunk(",\"remainPercent\":");
    sendUint8(channel.remainPercent);
    Esp32BaseWeb::sendChunk(",\"underflow\":");
    Esp32BaseWeb::sendChunk(channel.underflow ? "true" : "false");
    Esp32BaseWeb::sendChunk("}");
  }
  Esp32BaseWeb::sendChunk("]");
}

void sendTodaySummary(const FeederTodaySnapshot& snapshot) {
  Esp32BaseWeb::sendChunk("{\"serviceDate\":");
  sendUint32(snapshot.serviceDate);
  Esp32BaseWeb::sendChunk(",\"channels\":[");
  for (uint8_t i = 0; i < kFeederConfiguredChannels; ++i) {
    if (i > 0) {
      Esp32BaseWeb::sendChunk(",");
    }
    const FeederTodayChannel& channel = snapshot.channels[i];
    Esp32BaseWeb::sendChunk("{\"channel\":");
    sendUint8(i);
    Esp32BaseWeb::sendChunk(",\"pulses\":");
    char number[16];
    snprintf(number, sizeof(number), "%ld", static_cast<long>(channel.pulses));
    Esp32BaseWeb::sendChunk(number);
    Esp32BaseWeb::sendChunk(",\"gramsX100\":");
    snprintf(number, sizeof(number), "%ld", static_cast<long>(channel.gramsX100));
    Esp32BaseWeb::sendChunk(number);
    Esp32BaseWeb::sendChunk("}");
  }
  Esp32BaseWeb::sendChunk("]}");
}

void sendBaseInfoSummary(const FeederBucketSnapshot& snapshot) {
  Esp32BaseWeb::sendChunk("[");
  for (uint8_t i = 0; i < kFeederConfiguredChannels; ++i) {
    if (i > 0) {
      Esp32BaseWeb::sendChunk(",");
    }
    const FeederChannelBaseInfo& info = snapshot.channels[i].baseInfo;
    Esp32BaseWeb::sendChunk("{\"channel\":");
    sendUint8(i);
    Esp32BaseWeb::sendChunk(",\"enabled\":");
    Esp32BaseWeb::sendChunk(info.enabled ? "true" : "false");
    Esp32BaseWeb::sendChunk(",\"name\":\"通道 ");
    sendUint8(static_cast<uint8_t>(i + 1));
    Esp32BaseWeb::sendChunk("\",\"outputPulsesPerRev\":");
    char number[16];
    snprintf(number, sizeof(number), "%ld", static_cast<long>(info.outputPulsesPerRev));
    Esp32BaseWeb::sendChunk(number);
    Esp32BaseWeb::sendChunk(",\"gramsPerRevX100\":");
    snprintf(number, sizeof(number), "%ld", static_cast<long>(info.gramsPerRevX100));
    Esp32BaseWeb::sendChunk(number);
    Esp32BaseWeb::sendChunk(",\"capacityGramsX100\":");
    snprintf(number, sizeof(number), "%ld", static_cast<long>(info.capacityGramsX100));
    Esp32BaseWeb::sendChunk(number);
    Esp32BaseWeb::sendChunk("}");
  }
  Esp32BaseWeb::sendChunk("]");
}

const char* targetModeName(FeederTargetMode mode) {
  switch (mode) {
    case FeederTargetMode::None: return "None";
    case FeederTargetMode::Grams: return "Grams";
    case FeederTargetMode::Revolutions: return "Revolutions";
  }
  return "None";
}

const char* targetResultName(FeederTargetResult result) {
  switch (result) {
    case FeederTargetResult::Ok: return "Ok";
    case FeederTargetResult::InvalidArgument: return "InvalidArgument";
    case FeederTargetResult::NotCalibrated: return "NotCalibrated";
  }
  return "InvalidArgument";
}

void sendTargetSummary(const FeederTargetSnapshot& targets, const FeederBucketSnapshot& buckets) {
  Esp32BaseWeb::sendChunk("[");
  for (uint8_t i = 0; i < kFeederConfiguredChannels; ++i) {
    if (i > 0) {
      Esp32BaseWeb::sendChunk(",");
    }
    const FeederTargetRequest& target = targets.channels[i];
    const FeederChannelBaseInfo& info = buckets.channels[i].baseInfo;
    const FeederResolvedTarget resolved = resolveFeederTarget(info, target);
    Esp32BaseWeb::sendChunk("{\"channel\":");
    sendUint8(i);
    Esp32BaseWeb::sendChunk(",\"enabled\":");
    Esp32BaseWeb::sendChunk(info.enabled ? "true" : "false");
    Esp32BaseWeb::sendChunk(",\"targetMode\":\"");
    Esp32BaseWeb::sendChunk(targetModeName(target.mode));
    Esp32BaseWeb::sendChunk("\",\"targetGramsX100\":");
    char number[16];
    snprintf(number, sizeof(number), "%ld", static_cast<long>(target.targetGramsX100));
    Esp32BaseWeb::sendChunk(number);
    Esp32BaseWeb::sendChunk(",\"targetRevolutionsX100\":");
    snprintf(number, sizeof(number), "%ld", static_cast<long>(target.targetRevolutionsX100));
    Esp32BaseWeb::sendChunk(number);
    Esp32BaseWeb::sendChunk(",\"targetPulses\":");
    snprintf(number, sizeof(number), "%ld", static_cast<long>(resolved.targetPulses));
    Esp32BaseWeb::sendChunk(number);
    Esp32BaseWeb::sendChunk(",\"estimatedGramsX100\":");
    snprintf(number, sizeof(number), "%ld", static_cast<long>(resolved.estimatedGramsX100));
    Esp32BaseWeb::sendChunk(number);
    Esp32BaseWeb::sendChunk(",\"calibrated\":");
    Esp32BaseWeb::sendChunk(info.gramsPerRevX100 > 0 ? "true" : "false");
    Esp32BaseWeb::sendChunk("}");
  }
  Esp32BaseWeb::sendChunk("]");
}

void sendResolvedTargetArray(const FeederTargetBatch& batch) {
  Esp32BaseWeb::sendChunk("[");
  for (uint8_t i = 0; i < kFeederConfiguredChannels; ++i) {
    if (i > 0) {
      Esp32BaseWeb::sendChunk(",");
    }
    const FeederResolvedTarget& target = batch.channels[i];
    char number[16];
    Esp32BaseWeb::sendChunk("{\"channel\":");
    sendUint8(i);
    Esp32BaseWeb::sendChunk(",\"result\":\"");
    Esp32BaseWeb::sendChunk(targetResultName(target.result));
    Esp32BaseWeb::sendChunk("\",\"targetPulses\":");
    snprintf(number, sizeof(number), "%ld", static_cast<long>(target.targetPulses));
    Esp32BaseWeb::sendChunk(number);
    Esp32BaseWeb::sendChunk(",\"estimatedGramsX100\":");
    snprintf(number, sizeof(number), "%ld", static_cast<long>(target.estimatedGramsX100));
    Esp32BaseWeb::sendChunk(number);
    Esp32BaseWeb::sendChunk("}");
  }
  Esp32BaseWeb::sendChunk("]");
}

void sendRecordJson(const FeederRecord& record) {
  char number[16];
  Esp32BaseWeb::sendChunk("{\"sequence\":");
  sendUint32(record.sequence);
  Esp32BaseWeb::sendChunk(",\"unixTime\":");
  sendUint32(record.unixTime);
  Esp32BaseWeb::sendChunk(",\"uptimeSec\":");
  sendUint32(record.uptimeSec);
  Esp32BaseWeb::sendChunk(",\"bootId\":");
  sendUint32(record.bootId);
  Esp32BaseWeb::sendChunk(",\"commandId\":");
  sendUint32(record.commandId);
  Esp32BaseWeb::sendChunk(",\"eventType\":\"");
  Esp32BaseWeb::sendChunk(recordTypeName(record.type));
  Esp32BaseWeb::sendChunk("\",\"result\":\"");
  Esp32BaseWeb::sendChunk(recordResultName(record.result));
  Esp32BaseWeb::sendChunk("\",\"planId\":");
  sendUint8(record.planId);
  Esp32BaseWeb::sendChunk(",\"channel\":");
  sendUint8(record.channel);
  Esp32BaseWeb::sendChunk(",\"requestedMask\":");
  sendUint8(record.requestedMask);
  Esp32BaseWeb::sendChunk(",\"successMask\":");
  sendUint8(record.successMask);
  Esp32BaseWeb::sendChunk(",\"busyMask\":");
  sendUint8(record.busyMask);
  Esp32BaseWeb::sendChunk(",\"faultMask\":");
  sendUint8(record.faultMask);
  Esp32BaseWeb::sendChunk(",\"skippedMask\":");
  sendUint8(record.skippedMask);
  Esp32BaseWeb::sendChunk(",\"targetPulses\":");
  snprintf(number, sizeof(number), "%ld", static_cast<long>(record.targetPulses));
  Esp32BaseWeb::sendChunk(number);
  Esp32BaseWeb::sendChunk(",\"estimatedGramsX100\":");
  snprintf(number, sizeof(number), "%ld", static_cast<long>(record.estimatedGramsX100));
  Esp32BaseWeb::sendChunk(number);
  Esp32BaseWeb::sendChunk(",\"actualPulses\":");
  snprintf(number, sizeof(number), "%ld", static_cast<long>(record.actualPulses));
  Esp32BaseWeb::sendChunk(number);
  Esp32BaseWeb::sendChunk("}");
}

void sendRecordArray(const FeederRecordSnapshot& snapshot) {
  Esp32BaseWeb::sendChunk("[");
  for (uint8_t i = 0; i < snapshot.count; ++i) {
    if (i > 0) {
      Esp32BaseWeb::sendChunk(",");
    }
    sendRecordJson(snapshot.records[i]);
  }
  Esp32BaseWeb::sendChunk("]");
}

void sendRecordArray(const FeederRecordPage& page) {
  Esp32BaseWeb::sendChunk("[");
  for (uint8_t i = 0; i < page.count; ++i) {
    if (i > 0) {
      Esp32BaseWeb::sendChunk(",");
    }
    sendRecordJson(page.records[i]);
  }
  Esp32BaseWeb::sendChunk("]");
}

void sendRecentCommandJson() {
  const FeederRecordSnapshot records = g_records.snapshot();
  for (uint8_t i = records.count; i > 0; --i) {
    const FeederRecord& record = records.records[i - 1];
    if (record.commandId == 0) {
      continue;
    }
    Esp32BaseWeb::sendChunk("{\"commandId\":");
    sendUint32(record.commandId);
    Esp32BaseWeb::sendChunk(",\"sequence\":");
    sendUint32(record.sequence);
    Esp32BaseWeb::sendChunk(",\"unixTime\":");
    sendUint32(record.unixTime);
    Esp32BaseWeb::sendChunk(",\"eventType\":\"");
    Esp32BaseWeb::sendChunk(recordTypeName(record.type));
    Esp32BaseWeb::sendChunk("\",\"result\":\"");
    Esp32BaseWeb::sendChunk(recordResultName(record.result));
    Esp32BaseWeb::sendChunk("\",\"planId\":");
    sendUint8(record.planId);
    Esp32BaseWeb::sendChunk(",\"channel\":");
    sendUint8(record.channel);
    Esp32BaseWeb::sendChunk(",\"requestedMask\":");
    sendUint8(record.requestedMask);
    Esp32BaseWeb::sendChunk(",\"successMask\":");
    sendUint8(record.successMask);
    Esp32BaseWeb::sendChunk(",\"skippedMask\":");
    sendUint8(record.skippedMask);
    Esp32BaseWeb::sendChunk("}");
    return;
  }
  Esp32BaseWeb::sendChunk("null");
}

bool readUint8Param(const char* name, uint8_t& out) {
  char raw[12];
  if (!Esp32BaseWeb::getParam(name, raw, sizeof(raw))) {
    return false;
  }
  char* end = nullptr;
  const unsigned long value = strtoul(raw, &end, 10);
  if (!end || *end != '\0' || value > 255) {
    return false;
  }
  out = static_cast<uint8_t>(value);
  return true;
}

bool readUint32Param(const char* name, uint32_t& out) {
  char raw[16];
  if (!Esp32BaseWeb::getParam(name, raw, sizeof(raw))) {
    return false;
  }
  char* end = nullptr;
  const unsigned long value = strtoul(raw, &end, 10);
  if (!end || *end != '\0') {
    return false;
  }
  out = static_cast<uint32_t>(value);
  return true;
}

bool readUint32ParamOptional(const char* name, uint32_t& out) {
  return !Esp32BaseWeb::hasParam(name) || readUint32Param(name, out);
}

bool readInt32Param(const char* name, int32_t& out) {
  char raw[16];
  if (!Esp32BaseWeb::getParam(name, raw, sizeof(raw))) {
    return false;
  }
  char* end = nullptr;
  const long value = strtol(raw, &end, 10);
  if (!end || *end != '\0') {
    return false;
  }
  out = static_cast<int32_t>(value);
  return true;
}

bool readInt32ParamOptional(const char* name, int32_t& out) {
  return !Esp32BaseWeb::hasParam(name) || readInt32Param(name, out);
}

bool readBoolParam(const char* name, bool& out) {
  char raw[8];
  if (!Esp32BaseWeb::getParam(name, raw, sizeof(raw))) {
    return false;
  }
  if (strcmp(raw, "true") == 0 || strcmp(raw, "1") == 0) {
    out = true;
    return true;
  }
  if (strcmp(raw, "false") == 0 || strcmp(raw, "0") == 0) {
    out = false;
    return true;
  }
  return false;
}

bool parseTargetModeText(const char* raw, FeederTargetMode& out) {
  if (strcmp(raw, "grams") == 0 || strcmp(raw, "Grams") == 0) {
    out = FeederTargetMode::Grams;
    return true;
  }
  if (strcmp(raw, "revolutions") == 0 || strcmp(raw, "Revolutions") == 0) {
    out = FeederTargetMode::Revolutions;
    return true;
  }
  if (strcmp(raw, "none") == 0 || strcmp(raw, "None") == 0) {
    out = FeederTargetMode::None;
    return true;
  }
  return false;
}

bool readTargetModeParam(const char* name, FeederTargetMode& out) {
  char raw[16];
  if (!Esp32BaseWeb::getParam(name, raw, sizeof(raw))) {
    out = FeederTargetMode::None;
    return true;
  }
  return parseTargetModeText(raw, out);
}

const char* bucketResultName(FeederBucketResult result) {
  switch (result) {
    case FeederBucketResult::Ok: return "Ok";
    case FeederBucketResult::InvalidArgument: return "InvalidArgument";
    case FeederBucketResult::Underflow: return "Underflow";
  }
  return "InvalidArgument";
}

void sendResultJson(int code, const char* result, uint32_t commandId = 0) {
  Esp32BaseWeb::beginJson(code);
  Esp32BaseWeb::sendChunk("{\"result\":\"");
  Esp32BaseWeb::sendChunk(result);
  Esp32BaseWeb::sendChunk("\",\"commandId\":");
  sendUint32(commandId);
  Esp32BaseWeb::sendChunk("}");
  Esp32BaseWeb::endJson();
}

void sendConfirmRequired(const char* action, const char* resource) {
  char token[9] = {};
  if (!g_confirm.issue(action, resource, millis(), esp_random(), token, sizeof(token))) {
    sendResultJson(500, "InvalidArgument");
    return;
  }
  Esp32BaseWeb::beginJson(409);
  Esp32BaseWeb::sendChunk("{\"result\":\"ConfirmRequired\",\"actionId\":\"");
  Esp32BaseWeb::sendChunk(action);
  Esp32BaseWeb::sendChunk("\",\"resource\":\"");
  Esp32BaseWeb::sendChunk(resource);
  Esp32BaseWeb::sendChunk("\",\"confirmToken\":\"");
  Esp32BaseWeb::sendChunk(token);
  Esp32BaseWeb::sendChunk("\",\"ttlMs\":60000}");
  Esp32BaseWeb::endJson();
}

bool confirmAccepted() {
  bool confirm = false;
  return readBoolParam("confirm", confirm) && confirm;
}

bool requireConfirm(const char* action, const char* resource) {
  char token[16] = {};
  if (confirmAccepted() &&
      Esp32BaseWeb::getParam("confirmToken", token, sizeof(token)) &&
      g_confirm.consume(action, resource, token, millis())) {
    return true;
  }
  sendConfirmRequired(action, resource);
  return false;
}

void settleStoppedRuns(uint8_t channelMask) {
  bool changed = false;
  const FeederRunSnapshot runs = g_runs.snapshot();
  for (uint8_t i = 0; i < kFeederConfiguredChannels; ++i) {
    const uint8_t bit = static_cast<uint8_t>(1U << i);
    if ((channelMask & bit) == 0 || !runs.channels[i].active) {
      continue;
    }
    FeederRunChannel completed;
    if (!g_runs.finish(i, runs.channels[i].actualPulses, completed)) {
      continue;
    }
    FeederFeedSettlement settlement;
    const FeederFeedSettlementResult result =
        settleCompletedFeederRun(i, completed, g_today, g_buckets, settlement);
    if (result == FeederFeedSettlementResult::Ok ||
        result == FeederFeedSettlementResult::Underflow) {
      changed = true;
    }
  }
  if (changed) {
    persistFeederTodayIfReady();
    persistFeederBucketsIfReady();
  }
}

const char* scheduleResultName(FeederScheduleResult result) {
  switch (result) {
    case FeederScheduleResult::Ok: return "Ok";
    case FeederScheduleResult::Full: return "Full";
    case FeederScheduleResult::NotFound: return "NotFound";
    case FeederScheduleResult::InvalidArgument: return "InvalidArgument";
  }
  return "InvalidArgument";
}

const char* feederCommandResultName(FeederCommandResult result) {
  switch (result) {
    case FeederCommandResult::Ok: return "Ok";
    case FeederCommandResult::Partial: return "Partial";
    case FeederCommandResult::Busy: return "Busy";
    case FeederCommandResult::NotConfigured: return "NotConfigured";
    case FeederCommandResult::Fault: return "Fault";
    case FeederCommandResult::InvalidArgument: return "InvalidArgument";
  }
  return "InvalidArgument";
}

void sendStartResultJson(const FeederStartResult& result,
                         const FeederTargetBatch* targets = nullptr,
                         uint32_t commandId = 0) {
  const bool success = result.result == FeederCommandResult::Ok ||
                       result.result == FeederCommandResult::Partial;
  Esp32BaseWeb::beginJson(success ? 200 : 400);
  Esp32BaseWeb::sendChunk("{\"result\":\"");
  Esp32BaseWeb::sendChunk(feederCommandResultName(result.result));
  Esp32BaseWeb::sendChunk("\",\"commandId\":");
  sendUint32(commandId);
  Esp32BaseWeb::sendChunk(",\"successMask\":");
  sendUint8(result.successMask);
  Esp32BaseWeb::sendChunk(",\"busyMask\":");
  sendUint8(result.busyMask);
  Esp32BaseWeb::sendChunk(",\"faultMask\":");
  sendUint8(result.faultMask);
  Esp32BaseWeb::sendChunk(",\"skippedMask\":");
  sendUint8(result.skippedMask);
  if (targets) {
    Esp32BaseWeb::sendChunk(",\"targetOkMask\":");
    sendUint8(targets->okMask);
    Esp32BaseWeb::sendChunk(",\"targetInvalidMask\":");
    sendUint8(targets->invalidMask);
    Esp32BaseWeb::sendChunk(",\"targetNotCalibratedMask\":");
    sendUint8(targets->notCalibratedMask);
    Esp32BaseWeb::sendChunk(",\"targets\":");
    sendResolvedTargetArray(*targets);
  }
  Esp32BaseWeb::sendChunk(",\"motorOutput\":{\"enabled\":false}}");
  Esp32BaseWeb::endJson();
}

FeederStartResult startResolvedTargets(uint8_t channelMask,
                                       FeederRunSource source,
                                       uint32_t commandId,
                                       FeederTargetBatch& targets,
                                       const FeederTargetSnapshot* targetOverrides = nullptr) {
  const FeederTargetSnapshot storedTargets = g_targets.snapshot();
  targets = resolveFeederTargetsForMask(g_buckets.snapshot(),
                                        targetOverrides ? *targetOverrides : storedTargets,
                                        channelMask);
  if (targets.okMask == 0) {
    FeederStartResult result;
    result.result = FeederCommandResult::InvalidArgument;
    result.skippedMask = channelMask;
    return result;
  }
  FeederStartResult result = g_feeder.startChannels(targets.okMask, source);
  if (result.successMask != 0) {
    g_runs.start(result.successMask, source, targets);
    for (uint8_t i = 0; i < kFeederConfiguredChannels; ++i) {
      const uint8_t bit = static_cast<uint8_t>(1U << i);
      if ((result.successMask & bit) == 0) {
        continue;
      }
      FeederRecord record;
      record.commandId = commandId;
      record.type = FeederRecordType::ChannelStarted;
      record.result = FeederRecordResult::Ok;
      record.channel = i;
      record.successMask = bit;
      record.targetPulses = targets.channels[i].targetPulses;
      record.estimatedGramsX100 = targets.channels[i].estimatedGramsX100;
      recordBusinessEvent(record);
    }
  }
  result.skippedMask |= static_cast<uint8_t>(targets.invalidMask | targets.notCalibratedMask);
  if (result.successMask != 0 && result.skippedMask != 0 && result.result == FeederCommandResult::Ok) {
    result.result = FeederCommandResult::Partial;
  }
  return result;
}

bool readManualTargetOverrides(uint8_t channelMask, FeederTargetSnapshot& out) {
  out = g_targets.snapshot();
  for (uint8_t i = 0; i < kFeederConfiguredChannels; ++i) {
    const uint8_t bit = static_cast<uint8_t>(1U << i);
    if ((channelMask & bit) == 0) {
      continue;
    }
    char name[32];
    char rawMode[20];
    snprintf(name, sizeof(name), "ch%uMode", static_cast<unsigned>(i + 1));
    if (!Esp32BaseWeb::getParam(name, rawMode, sizeof(rawMode)) || rawMode[0] == '\0') {
      continue;
    }
    FeederTargetMode mode = FeederTargetMode::None;
    if (!parseTargetModeText(rawMode, mode) || mode == FeederTargetMode::None) {
      return false;
    }

    FeederTargetRequest request;
    request.mode = mode;
    snprintf(name, sizeof(name), "ch%uGramsX100", static_cast<unsigned>(i + 1));
    if (!readInt32ParamOptional(name, request.targetGramsX100)) {
      return false;
    }
    snprintf(name, sizeof(name), "ch%uRevolutionsX100", static_cast<unsigned>(i + 1));
    if (!readInt32ParamOptional(name, request.targetRevolutionsX100)) {
      return false;
    }
    if (mode == FeederTargetMode::Grams && request.targetGramsX100 <= 0) {
      return false;
    }
    if (mode == FeederTargetMode::Revolutions && request.targetRevolutionsX100 <= 0) {
      return false;
    }
    out.channels[i] = request;
  }
  return true;
}

FeederTargetSnapshot targetSnapshotFromPlan(const FeederPlanConfig& plan) {
  FeederTargetSnapshot targets;
  for (uint8_t i = 0; i < kFeederConfiguredChannels; ++i) {
    targets.channels[i].mode = plan.targets[i].mode;
    targets.channels[i].targetGramsX100 = plan.targets[i].targetGramsX100;
    targets.channels[i].targetRevolutionsX100 = plan.targets[i].targetRevolutionsX100;
  }
  return targets;
}

bool findPlanConfig(uint8_t planId, FeederPlanConfig& out) {
  const FeederScheduleSnapshot snapshot = g_schedules.snapshot();
  for (uint8_t i = 0; i < snapshot.planCount; ++i) {
    if (snapshot.plans[i].config.planId == planId) {
      out = snapshot.plans[i].config;
      return true;
    }
  }
  return false;
}

FeederStartResult startPlanTargets(const FeederPlanConfig& plan, FeederTargetBatch& targets) {
  targets = resolveFeederTargetsForMask(g_buckets.snapshot(), targetSnapshotFromPlan(plan), plan.channelMask);
  FeederStartResult result;
  if (targets.okMask != 0) {
    result = g_feeder.startChannels(targets.okMask, FeederRunSource::Schedule);
    if (result.successMask != 0) {
      g_runs.start(result.successMask, FeederRunSource::Schedule, targets);
    }
  } else {
    result.result = FeederCommandResult::InvalidArgument;
  }
  result.skippedMask |= static_cast<uint8_t>(
      plan.channelMask & static_cast<uint8_t>(targets.invalidMask | targets.notCalibratedMask));
  if (result.successMask != 0 && result.skippedMask != 0 && result.result == FeederCommandResult::Ok) {
    result.result = FeederCommandResult::Partial;
  }
  return result;
}

bool readPlanConfigFromParams(FeederPlanConfig& config) {
  int32_t raw = 0;
  if (!readBoolParam("enabled", config.enabled)) {
    return false;
  }
  config.timeConfigured = Esp32BaseWeb::hasParam("timeMinutes");
  if (config.timeConfigured) {
    if (!readInt32Param("timeMinutes", raw) || raw < 0 || raw >= 24 * 60) {
      return false;
    }
    config.timeMinutes = static_cast<uint16_t>(raw);
  }
  if (Esp32BaseWeb::hasParam("channelMask")) {
    if (!readInt32Param("channelMask", raw) || raw < 0 || raw > 15) {
      return false;
    }
    config.channelMask = static_cast<uint8_t>(raw);
  }

  for (uint8_t i = 0; i < kFeederConfiguredChannels; ++i) {
    char name[28];
    snprintf(name, sizeof(name), "ch%uMode", static_cast<unsigned>(i));
    if (!readTargetModeParam(name, config.targets[i].mode)) {
      return false;
    }
    snprintf(name, sizeof(name), "ch%uGramsX100", static_cast<unsigned>(i));
    if (!readInt32ParamOptional(name, config.targets[i].targetGramsX100)) {
      return false;
    }
    snprintf(name, sizeof(name), "ch%uRevolutionsX100", static_cast<unsigned>(i));
    if (!readInt32ParamOptional(name, config.targets[i].targetRevolutionsX100)) {
      return false;
    }
  }
  return true;
}

}  // namespace

FarmFeederApp FarmFeeder;

void FarmFeederApp::begin() {
  Serial.begin(115200);
  Wire.begin(kFeederI2cSda, kFeederI2cScl);
  configureStaticDefaults();

  Esp32Base::setFirmwareInfo("Esp32FarmFeeder", "0.1.0");
  configureAppConfigPage();
  configureBusinessShell();

  if (!Esp32Base::begin()) {
    ESP32BASE_LOG_E("farmfeeder", "Esp32Base begin failed: %s", Esp32Base::lastError());
  } else {
    initializeFeederAt24cStore();
    ESP32BASE_LOG_I("farmfeeder", "skeleton ready enabled_mask=%u", g_feederConfig.enabledChannelMask);
  }
}

void FarmFeederApp::handle() {
  Esp32Base::handle();
  handleScheduleTick();
}

void FarmFeederApp::handleScheduleTick() {
#if ESP32BASE_ENABLE_NTP
  const Esp32BaseNtp::TimeSnapshot timeSnapshot = Esp32BaseNtp::snapshot();
  if (!timeSnapshot.synced || timeSnapshot.epochSec == 0) {
    return;
  }

  uint32_t serviceDate = 0;
  uint16_t currentMinute = 0;
  if (!feederLocalDateAndMinute(timeSnapshot.epochSec, serviceDate, currentMinute)) {
    return;
  }

  if (serviceDate != g_lastScheduleDate) {
    g_schedules.beginDay(serviceDate);
    g_today.beginDay(serviceDate);
    g_lastScheduleDate = serviceDate;
    g_lastScheduleMinute = 24 * 60;
    persistFeederScheduleIfReady();
    persistFeederTodayIfReady();
  }
  if (currentMinute == g_lastScheduleMinute) {
    return;
  }
  g_lastScheduleMinute = currentMinute;

  const FeederScheduleTick tick = g_schedules.evaluate(currentMinute, true);
  if (tick.action == FeederScheduleAction::NoAction) {
    return;
  }
  if (tick.action == FeederScheduleAction::MarkMissed) {
    FeederRecord record;
    record.type = FeederRecordType::ScheduleMissed;
    record.result = FeederRecordResult::Skipped;
    record.planId = tick.planId;
    recordBusinessEvent(record);
    ESP32BASE_LOG_W("farmfeeder", "schedule_missed plan_id=%u date=%lu minute=%u",
                    static_cast<unsigned>(tick.planId),
                    static_cast<unsigned long>(serviceDate),
                    static_cast<unsigned>(currentMinute));
    persistFeederScheduleIfReady();
    return;
  }

  FeederPlanConfig plan;
  if (!findPlanConfig(tick.planId, plan)) {
    ESP32BASE_LOG_W("farmfeeder", "schedule_plan_missing plan_id=%u",
                    static_cast<unsigned>(tick.planId));
    return;
  }

  g_schedules.markAttempted(tick.planId);
  FeederTargetBatch targets;
  const uint32_t commandId = allocateCommandId();
  const FeederStartResult result = startPlanTargets(plan, targets);
  FeederRecord record;
  record.commandId = commandId;
  record.type = FeederRecordType::ScheduleTriggered;
  record.result = feederRecordResultFromCommand(result.result);
  record.planId = tick.planId;
  record.requestedMask = plan.channelMask;
  record.successMask = result.successMask;
  record.busyMask = result.busyMask;
  record.faultMask = result.faultMask;
  record.skippedMask = result.skippedMask;
  recordBusinessEvent(record);
  persistFeederScheduleIfReady();
  ESP32BASE_LOG_I("farmfeeder",
                  "schedule_triggered plan_id=%u result=%u success_mask=%u skipped_mask=%u",
                  static_cast<unsigned>(tick.planId),
                  static_cast<unsigned>(result.result),
                  static_cast<unsigned>(result.successMask),
                  static_cast<unsigned>(result.skippedMask));
#endif
}

void FarmFeederApp::configureStaticDefaults() {
  g_feederConfig.installedChannelMask = configuredChannelMask();
  g_feederConfig.enabledChannelMask = configuredChannelMask();

  g_schedules.beginDay(0);
  g_today.beginDay(0);

  FeederChannelBaseInfo channelInfo;
  channelInfo.enabled = true;
  channelInfo.outputPulsesPerRev = 4320;
  channelInfo.gramsPerRevX100 = 7000;
  channelInfo.capacityGramsX100 = 500000;
  for (uint8_t i = 0; i < kFeederConfiguredChannels; ++i) {
    if (g_buckets.updateBaseInfo(i, channelInfo) != FeederBucketResult::Ok) {
      ESP32BASE_LOG_E("farmfeeder", "bucket_base_info_failed channel=%u", static_cast<unsigned>(i));
    }
    FeederTargetRequest target;
    target.mode = FeederTargetMode::Grams;
    target.targetGramsX100 = 7000;
    if (g_targets.setTarget(i, target) != FeederTargetResult::Ok) {
      ESP32BASE_LOG_E("farmfeeder", "target_default_failed channel=%u", static_cast<unsigned>(i));
    }
  }
  syncFeederConfigFromBaseInfo();
}

void FarmFeederApp::configureAppConfigPage() {
#if ESP32BASE_ENABLE_APP_CONFIG
  Esp32BaseAppConfig::setTitle("Esp32FarmFeeder 参数");
#endif
}

void FarmFeederApp::configureBusinessShell() {
#if ESP32BASE_ENABLE_WEB
  Esp32BaseWeb::setDeviceName("Esp32FarmFeeder");
  Esp32BaseWeb::setHomePath("/");
  Esp32BaseWeb::setHomeMode(Esp32BaseWeb::HOME_APP);
  Esp32BaseWeb::setSystemNavMode(Esp32BaseWeb::SYSTEM_NAV_BOTTOM);
  Esp32BaseWeb::addNavItem("/", "首页");
  Esp32BaseWeb::addNavItem("/schedule", "计划");
  Esp32BaseWeb::addNavItem("/records", "记录");
  Esp32BaseWeb::addNavItem("/base-info", "基础信息");
  Esp32BaseWeb::addNavItem("/diagnostics", "诊断");
  Esp32BaseWeb::addPage("/", "喂食器首页", FarmFeederApp::sendHomePage);
  Esp32BaseWeb::addPage("/schedule", "喂食计划", FarmFeederApp::sendSchedulePage);
  Esp32BaseWeb::addPage("/records", "喂食记录", FarmFeederApp::sendRecordsPage);
  Esp32BaseWeb::addPage("/base-info", "基础信息", FarmFeederApp::sendBaseInfoPage);
  Esp32BaseWeb::addPage("/diagnostics", "喂食器诊断", FarmFeederApp::sendDiagnosticsPage);
  addFarmFeederApi("/api/app/status", FarmFeederApp::sendStatusJson);
  addFarmFeederApi("/api/app/diagnostics", FarmFeederApp::sendDiagnosticsJson);
  addFarmFeederApi("/api/app/events/recent", FarmFeederApp::sendRecentEventsJson);
  addFarmFeederApi("/api/app/feeders/manual-start", FarmFeederApp::handleFeederManualStart);
  addFarmFeederApi("/api/app/feeders/start", FarmFeederApp::handleFeederStart);
  addFarmFeederApi("/api/app/feeders/stop", FarmFeederApp::handleFeederStop);
  addFarmFeederApi("/api/app/feeders/stop-all", FarmFeederApp::handleFeederStopAll);
  addFarmFeederApi("/api/app/feeders/targets", FarmFeederApp::sendTargetsJson);
  addFarmFeederApi("/api/app/feeders/target", FarmFeederApp::handleFeederTarget);
  addFarmFeederApi("/api/app/schedules", FarmFeederApp::sendSchedulesJson);
  addFarmFeederApi("/api/app/schedules/create", FarmFeederApp::handleScheduleCreate);
  addFarmFeederApi("/api/app/schedules/update", FarmFeederApp::handleScheduleUpdate);
  addFarmFeederApi("/api/app/schedules/delete", FarmFeederApp::handleScheduleDelete);
  addFarmFeederApi("/api/app/schedule-occurrence/skip", FarmFeederApp::handleScheduleSkip);
  addFarmFeederApi("/api/app/schedule-occurrence/cancel-skip", FarmFeederApp::handleScheduleCancelSkip);
  addFarmFeederApi("/api/app/buckets", FarmFeederApp::sendBucketsJson);
  addFarmFeederApi("/api/app/buckets/set-remaining", FarmFeederApp::handleBucketSetRemaining);
  addFarmFeederApi("/api/app/buckets/add-feed", FarmFeederApp::handleBucketAddFeed);
  addFarmFeederApi("/api/app/buckets/mark-full", FarmFeederApp::handleBucketMarkFull);
  addFarmFeederApi("/api/app/base-info", FarmFeederApp::sendBaseInfoJson);
  addFarmFeederApi("/api/app/base-info/channel", FarmFeederApp::handleBaseInfoChannel);
  addFarmFeederApi("/api/app/records", FarmFeederApp::sendRecordsJson);
  addFarmFeederApi("/api/app/maintenance/clear-today", FarmFeederApp::handleMaintenanceClearToday);
  addFarmFeederApi("/api/app/maintenance/clear-fault", FarmFeederApp::handleMaintenanceClearFault);
#endif
}

void FarmFeederApp::sendHomePage() {
#if ESP32BASE_ENABLE_WEB
  const FeederSnapshot snapshot = g_feeder.snapshot();
  const FeederBucketSnapshot buckets = g_buckets.snapshot();
  Esp32BaseWeb::sendHeader("喂食器首页");
  Esp32BaseWeb::sendChunk("<h1>喂食器首页</h1><section><h2>喂食状态</h2><table>");
  Esp32BaseWeb::sendChunk("<tr><th>设备状态</th><td>");
  Esp32BaseWeb::sendChunk(deviceStateName(snapshot.state));
  Esp32BaseWeb::sendChunk("</td></tr><tr><th>运行通道</th><td>");
  sendUint8(snapshot.runningChannelMask);
  Esp32BaseWeb::sendChunk("</td></tr></table></section><section><h2>手工喂食</h2>");
  Esp32BaseWeb::sendChunk("<form method='post' action='/api/app/feeders/manual-start'>");
  Esp32BaseWeb::sendChunk("<label>通道掩码 <input name='channelMask' value='7'></label> ");
  Esp32BaseWeb::sendChunk("<p>可选本次目标覆盖，不填写则使用已保存的默认目标。</p>");
  Esp32BaseWeb::sendChunk("<label>通道1模式 <input name='ch1Mode' placeholder='Grams/Revolutions'></label> ");
  Esp32BaseWeb::sendChunk("<label>通道1克数x100 <input name='ch1GramsX100'></label> ");
  Esp32BaseWeb::sendChunk("<label>通道1圈数x100 <input name='ch1RevolutionsX100'></label> ");
  Esp32BaseWeb::sendChunk("<label>通道2模式 <input name='ch2Mode' placeholder='Grams/Revolutions'></label> ");
  Esp32BaseWeb::sendChunk("<label>通道2克数x100 <input name='ch2GramsX100'></label> ");
  Esp32BaseWeb::sendChunk("<label>通道2圈数x100 <input name='ch2RevolutionsX100'></label> ");
  Esp32BaseWeb::sendChunk("<label>通道3模式 <input name='ch3Mode' placeholder='Grams/Revolutions'></label> ");
  Esp32BaseWeb::sendChunk("<label>通道3克数x100 <input name='ch3GramsX100'></label> ");
  Esp32BaseWeb::sendChunk("<label>通道3圈数x100 <input name='ch3RevolutionsX100'></label> ");
  Esp32BaseWeb::sendChunk("<button>开始手工喂食</button></form>");
  Esp32BaseWeb::sendChunk("<form method='post' action='/api/app/feeders/stop-all'><button>停止全部</button></form>");
  Esp32BaseWeb::sendChunk("</section><section><h2>料桶余量</h2><table><tr><th>通道</th><th>当前估算</th><th>满桶容量</th></tr>");
  for (uint8_t i = 0; i < kFeederConfiguredChannels; ++i) {
    Esp32BaseWeb::sendChunk("<tr><td>");
    sendUint8(static_cast<uint8_t>(i + 1));
    Esp32BaseWeb::sendChunk("</td><td>");
    sendInt32(buckets.channels[i].remainGramsX100);
    Esp32BaseWeb::sendChunk(" / 100 g</td><td>");
    sendInt32(buckets.channels[i].baseInfo.capacityGramsX100);
    Esp32BaseWeb::sendChunk(" / 100 g</td></tr>");
  }
  Esp32BaseWeb::sendChunk("</table></section>");
  Esp32BaseWeb::sendFooter();
#endif
}

void FarmFeederApp::sendSchedulePage() {
#if ESP32BASE_ENABLE_WEB
  const FeederScheduleSnapshot schedules = g_schedules.snapshot();
  uint32_t tomorrowDate = 0;
  feederNextServiceDate(schedules.serviceDate, tomorrowDate);
  Esp32BaseWeb::sendHeader("计划");
  Esp32BaseWeb::sendChunk("<h1>计划</h1>");
  sendOccurrenceTable(schedules, schedules.serviceDate, "今日计划");
  sendOccurrenceTable(schedules, tomorrowDate, "明日计划");
  Esp32BaseWeb::sendChunk("<section><h2>计划管理</h2><p>计划管理只维护长期蓝本；跳过只在今日或明日执行实例上操作。</p>");
  Esp32BaseWeb::sendChunk("<p><a href='/api/app/schedules'>查看计划 JSON</a></p>");
  Esp32BaseWeb::sendChunk("<table><tr><th>ID</th><th>名称</th><th>时间</th><th>启用</th><th>目标</th></tr>");
  for (uint8_t i = 0; i < schedules.planCount; ++i) {
    const FeederPlanState& plan = schedules.plans[i];
    Esp32BaseWeb::sendChunk("<tr><td>");
    sendUint8(plan.config.planId);
    Esp32BaseWeb::sendChunk("</td><td>计划 ");
    sendUint8(plan.config.planId);
    Esp32BaseWeb::sendChunk("</td><td>");
    sendPlanTime(plan.config.timeMinutes);
    Esp32BaseWeb::sendChunk("</td><td>");
    Esp32BaseWeb::sendChunk(plan.config.enabled ? "是" : "否");
    Esp32BaseWeb::sendChunk("</td><td>");
    sendPlanTargetSummary(plan.config);
    Esp32BaseWeb::sendChunk("</td></tr>");
  }
  Esp32BaseWeb::sendChunk("</table></section>");
  Esp32BaseWeb::sendFooter();
#endif
}

void FarmFeederApp::sendRecordsPage() {
#if ESP32BASE_ENABLE_WEB
  Esp32BaseWeb::sendHeader("喂食记录");
  Esp32BaseWeb::sendChunk("<h1>记录</h1><section><h2>业务记录查询</h2>");
  Esp32BaseWeb::sendChunk("<form method='get' action='/api/app/records'>");
  Esp32BaseWeb::sendChunk("<label>开始序号 <input name='start' value='0'></label> ");
  Esp32BaseWeb::sendChunk("<label>每页 <input name='limit' value='16'></label> ");
  Esp32BaseWeb::sendChunk("<label>归档 <input name='archive' value='0'></label> ");
  Esp32BaseWeb::sendChunk("<label>开始时间戳 <input name='startUnixTime'></label> ");
  Esp32BaseWeb::sendChunk("<label>结束时间戳 <input name='endUnixTime'></label> ");
  Esp32BaseWeb::sendChunk("<label>事件类型 <input name='eventType' placeholder='FeederManualRequested'></label> ");
  Esp32BaseWeb::sendChunk("<button>查询 JSON</button></form>");
  Esp32BaseWeb::sendChunk("<p>归档 0 表示当前文件，1-16 表示轮转归档文件。</p></section>");
  Esp32BaseWeb::sendFooter();
#endif
}

void FarmFeederApp::sendBaseInfoPage() {
#if ESP32BASE_ENABLE_WEB
  const FeederBucketSnapshot buckets = g_buckets.snapshot();
  Esp32BaseWeb::sendHeader("基础信息");
  Esp32BaseWeb::sendChunk("<h1>基础信息</h1><section><h2>通道基础信息</h2><table>");
  Esp32BaseWeb::sendChunk("<tr><th>通道</th><th>名称</th><th>启用</th><th>每圈信号数</th><th>每圈克数</th></tr>");
  for (uint8_t i = 0; i < kFeederConfiguredChannels; ++i) {
    Esp32BaseWeb::sendChunk("<tr><td>");
    sendUint8(static_cast<uint8_t>(i + 1));
    Esp32BaseWeb::sendChunk("</td><td>通道 ");
    sendUint8(static_cast<uint8_t>(i + 1));
    Esp32BaseWeb::sendChunk("</td><td>");
    Esp32BaseWeb::sendChunk(buckets.channels[i].baseInfo.enabled ? "是" : "否");
    Esp32BaseWeb::sendChunk("</td><td>");
    sendInt32(buckets.channels[i].baseInfo.outputPulsesPerRev);
    Esp32BaseWeb::sendChunk("</td><td>");
    sendInt32(buckets.channels[i].baseInfo.gramsPerRevX100);
    Esp32BaseWeb::sendChunk(" / 100 g</td></tr>");
  }
  Esp32BaseWeb::sendChunk("</table></section><section><h2>修改单路信息</h2>");
  Esp32BaseWeb::sendChunk("<form method='post' action='/api/app/base-info/channel'>");
  Esp32BaseWeb::sendChunk("<label>通道 <input name='channel' value='0'></label> ");
  Esp32BaseWeb::sendChunk("<label>启用 <input name='enabled' value='1'></label> ");
  Esp32BaseWeb::sendChunk("<label>每圈信号数 <input name='outputPulsesPerRev' value='4320'></label> ");
  Esp32BaseWeb::sendChunk("<label>每圈克数 x100 <input name='gramsPerRevX100' value='7000'></label> ");
  Esp32BaseWeb::sendChunk("<label>满桶容量 x100 <input name='capacityGramsX100' value='500000'></label> ");
  Esp32BaseWeb::sendChunk("<label>确认 token <input name='confirmToken'></label> ");
  Esp32BaseWeb::sendChunk("<label>确认 <input name='confirm' value='true'></label> ");
  Esp32BaseWeb::sendChunk("<button>保存</button></form></section>");
  Esp32BaseWeb::sendFooter();
#endif
}

void FarmFeederApp::sendDiagnosticsPage() {
#if ESP32BASE_ENABLE_WEB
  Esp32BaseWeb::sendHeader("喂食器诊断");
  Esp32BaseWeb::sendChunk("<h1>诊断</h1><section><h2>业务诊断</h2>");
  Esp32BaseWeb::sendChunk("<p><a href='/api/app/diagnostics'>查看诊断 JSON</a></p>");
  Esp32BaseWeb::sendChunk("<p><a href='/api/app/status'>查看状态 JSON</a></p>");
  Esp32BaseWeb::sendChunk("</section><section><h2>维护动作</h2>");
  Esp32BaseWeb::sendChunk("<form method='post' action='/api/app/maintenance/clear-today'>");
  Esp32BaseWeb::sendChunk("<label>确认 token <input name='confirmToken'></label> ");
  Esp32BaseWeb::sendChunk("<label>确认 <input name='confirm' value='true'></label> ");
  Esp32BaseWeb::sendChunk("<button>清空今日执行状态</button></form>");
  Esp32BaseWeb::sendChunk("<form method='post' action='/api/app/maintenance/clear-fault'>");
  Esp32BaseWeb::sendChunk("<label>通道掩码 <input name='channelMask' value='7'></label> ");
  Esp32BaseWeb::sendChunk("<label>确认 token <input name='confirmToken'></label> ");
  Esp32BaseWeb::sendChunk("<label>确认 <input name='confirm' value='true'></label> ");
  Esp32BaseWeb::sendChunk("<button>清除通道故障</button></form>");
  Esp32BaseWeb::sendChunk("</section>");
  Esp32BaseWeb::sendFooter();
#endif
}

void FarmFeederApp::sendStatusJson() {
#if ESP32BASE_ENABLE_WEB
  const FeederSnapshot snapshot = g_feeder.snapshot();
  const FeederRunSnapshot runSnapshot = g_runs.snapshot();
  const FeederScheduleSnapshot scheduleSnapshot = g_schedules.snapshot();
  const FeederBucketSnapshot bucketSnapshot = g_buckets.snapshot();

  Esp32BaseWeb::beginJson(200);
  Esp32BaseWeb::sendChunk("{\"appKind\":\"FarmFeeder\",");
  Esp32BaseWeb::sendChunk("\"firmware\":\"");
  Esp32BaseWeb::writeJsonEscaped(Esp32Base::firmwareVersion());
  Esp32BaseWeb::sendChunk("\",\"schemaVersion\":1,");
  Esp32BaseWeb::sendChunk("\"state\":\"");
  Esp32BaseWeb::sendChunk(deviceStateName(snapshot.state));
  Esp32BaseWeb::sendChunk("\",\"installedChannelMask\":");
  sendUint8(snapshot.installedChannelMask);
  Esp32BaseWeb::sendChunk(",\"enabledChannelMask\":");
  sendUint8(snapshot.enabledChannelMask);
  Esp32BaseWeb::sendChunk(",\"runningChannelMask\":");
  sendUint8(snapshot.runningChannelMask);
  Esp32BaseWeb::sendChunk(",\"faultChannelMask\":");
  sendUint8(snapshot.faultChannelMask);
  Esp32BaseWeb::sendChunk(",\"runningCount\":");
  sendUint8(snapshot.runningCount);
  Esp32BaseWeb::sendChunk(",\"channels\":");
  sendChannelArray(snapshot, runSnapshot);
  Esp32BaseWeb::sendChunk(",\"schedule\":");
  sendScheduleSummary(scheduleSnapshot);
  Esp32BaseWeb::sendChunk(",\"buckets\":");
  sendBucketSummary(bucketSnapshot);
  Esp32BaseWeb::sendChunk(",\"today\":");
  sendTodaySummary(g_today.snapshot());
  Esp32BaseWeb::sendChunk(",\"recentCommand\":");
  sendRecentCommandJson();
  Esp32BaseWeb::sendChunk(",\"motorOutput\":{\"enabled\":false}}");
  Esp32BaseWeb::endJson();
#endif
}

void FarmFeederApp::sendDiagnosticsJson() {
#if ESP32BASE_ENABLE_WEB
  const FeederSnapshot snapshot = g_feeder.snapshot();
  const FeederScheduleSnapshot scheduleSnapshot = g_schedules.snapshot();
  const FeederBucketSnapshot bucketSnapshot = g_buckets.snapshot();
  const FeederTargetSnapshot targetSnapshot = g_targets.snapshot();
  const FeederRecordSnapshot recordSnapshot = g_records.snapshot();

  Esp32BaseWeb::beginJson(200);
  Esp32BaseWeb::sendChunk("{\"appKind\":\"FarmFeeder\",\"mode\":\"readOnlyDiagnostics\"");
  Esp32BaseWeb::sendChunk(",\"state\":\"");
  Esp32BaseWeb::sendChunk(deviceStateName(snapshot.state));
  Esp32BaseWeb::sendChunk("\",\"installedChannelMask\":");
  sendUint8(snapshot.installedChannelMask);
  Esp32BaseWeb::sendChunk(",\"enabledChannelMask\":");
  sendUint8(snapshot.enabledChannelMask);
  Esp32BaseWeb::sendChunk(",\"runningChannelMask\":");
  sendUint8(snapshot.runningChannelMask);
  Esp32BaseWeb::sendChunk(",\"faultChannelMask\":");
  sendUint8(snapshot.faultChannelMask);
  Esp32BaseWeb::sendChunk(",\"schedule\":");
  sendScheduleSummary(scheduleSnapshot);
  Esp32BaseWeb::sendChunk(",\"buckets\":");
  sendBucketSummary(bucketSnapshot);
  Esp32BaseWeb::sendChunk(",\"targets\":");
  sendTargetSummary(targetSnapshot, bucketSnapshot);
  Esp32BaseWeb::sendChunk(",\"records\":{\"recentCount\":");
  sendUint8(recordSnapshot.count);
  Esp32BaseWeb::sendChunk(",\"recentCapacity\":");
  sendUint8(kFeederRecentRecordCapacity);
  Esp32BaseWeb::sendChunk(",\"flashReady\":");
#if ESP32BASE_ENABLE_FS
  Esp32BaseWeb::sendChunk(Esp32BaseFs::isReady() ? "true" : "false");
#else
  Esp32BaseWeb::sendChunk("false");
#endif
  Esp32BaseWeb::sendChunk("},\"currentSensors\":{\"installed\":false},");
  Esp32BaseWeb::sendChunk("\"at24c\":{\"storeReady\":");
  Esp32BaseWeb::sendChunk(g_at24cStoreReady ? "true" : "false");
  Esp32BaseWeb::sendChunk(",\"address\":\"0x50\"},");
  Esp32BaseWeb::sendChunk("\"motorOutput\":{\"enabled\":false}}");
  Esp32BaseWeb::endJson();
#endif
}

void FarmFeederApp::sendRecentEventsJson() {
#if ESP32BASE_ENABLE_WEB
  const FeederRecordSnapshot snapshot = g_records.snapshot();
  Esp32BaseWeb::beginJson(200);
  Esp32BaseWeb::sendChunk("{\"source\":\"ram\",\"count\":");
  sendUint8(snapshot.count);
  Esp32BaseWeb::sendChunk(",\"capacity\":");
  sendUint8(kFeederRecentRecordCapacity);
  Esp32BaseWeb::sendChunk(",\"records\":");
  sendRecordArray(snapshot);
  Esp32BaseWeb::sendChunk("}");
  Esp32BaseWeb::endJson();
#endif
}

void FarmFeederApp::sendSchedulesJson() {
#if ESP32BASE_ENABLE_WEB
  Esp32BaseWeb::beginJson(200);
  sendScheduleSummary(g_schedules.snapshot());
  Esp32BaseWeb::endJson();
#endif
}

void FarmFeederApp::sendBucketsJson() {
#if ESP32BASE_ENABLE_WEB
  Esp32BaseWeb::beginJson(200);
  Esp32BaseWeb::sendChunk("{\"channels\":");
  sendBucketSummary(g_buckets.snapshot());
  Esp32BaseWeb::sendChunk("}");
  Esp32BaseWeb::endJson();
#endif
}

void FarmFeederApp::sendBaseInfoJson() {
#if ESP32BASE_ENABLE_WEB
  Esp32BaseWeb::beginJson(200);
  Esp32BaseWeb::sendChunk("{\"channels\":");
  sendBaseInfoSummary(g_buckets.snapshot());
  Esp32BaseWeb::sendChunk("}");
  Esp32BaseWeb::endJson();
#endif
}

void FarmFeederApp::sendTargetsJson() {
#if ESP32BASE_ENABLE_WEB
  Esp32BaseWeb::beginJson(200);
  Esp32BaseWeb::sendChunk("{\"channels\":");
  sendTargetSummary(g_targets.snapshot(), g_buckets.snapshot());
  Esp32BaseWeb::sendChunk("}");
  Esp32BaseWeb::endJson();
#endif
}

void FarmFeederApp::sendRecordsJson() {
#if ESP32BASE_ENABLE_WEB
  FeederRecordQuery query;
  uint32_t limitParam = query.limit;
  if (!readUint32ParamOptional("start", query.startIndex) ||
      !readUint32ParamOptional("limit", limitParam) ||
      !readUint32ParamOptional("startUnixTime", query.startUnixTime) ||
      !readUint32ParamOptional("endUnixTime", query.endUnixTime) || limitParam == 0 ||
      limitParam > kFeederRecordPageMaxRecords) {
    sendResultJson(400, "InvalidArgument");
    return;
  }
  query.limit = static_cast<uint8_t>(limitParam);
  char eventType[32];
  if (Esp32BaseWeb::getParam("eventType", eventType, sizeof(eventType)) && eventType[0] != '\0') {
    if (!recordTypeFromName(eventType, query.type)) {
      sendResultJson(400, "InvalidArgument");
      return;
    }
    query.typeFilterEnabled = true;
  }
  uint32_t archiveIndex = 0;
  if (!readUint32ParamOptional("archive", archiveIndex) || archiveIndex > kFeederRecordMaxArchives) {
    sendResultJson(400, "InvalidArgument");
    return;
  }

  Esp32BaseWeb::beginJson(200);
#if ESP32BASE_ENABLE_FS
  char recordPath[96];
  if (!feederRecordPathForArchive(archiveIndex, recordPath, sizeof(recordPath))) {
    sendResultJson(400, "InvalidArgument");
    return;
  }
  FeederRecordPage page;
  const FeederRecordReadResult readResult = readFeederRecordPage(recordPath,
                                                                 query,
                                                                 feederRecordFileSize,
                                                                 readFeederRecordBytesAt,
                                                                 nullptr,
                                                                 page);
  if (readResult == FeederRecordReadResult::Ok && page.totalRecords > 0) {
    Esp32BaseWeb::sendChunk("{\"source\":\"flash\",\"start\":");
    sendUint32(page.startIndex);
    Esp32BaseWeb::sendChunk(",\"archive\":");
    sendUint32(archiveIndex);
    Esp32BaseWeb::sendChunk(",\"nextIndex\":");
    sendUint32(page.nextIndex);
    Esp32BaseWeb::sendChunk(",\"limit\":");
    sendUint32(limitParam);
    Esp32BaseWeb::sendChunk(",\"count\":");
    sendUint8(page.count);
    Esp32BaseWeb::sendChunk(",\"totalRecords\":");
    sendUint32(page.totalRecords);
    Esp32BaseWeb::sendChunk(",\"recordBytes\":");
    sendUint32(kFeederRecordEncodedMaxBytes);
    Esp32BaseWeb::sendChunk(",\"records\":");
    sendRecordArray(page);
    Esp32BaseWeb::sendChunk("}");
    Esp32BaseWeb::endJson();
    return;
  }
  if (archiveIndex > 0) {
    Esp32BaseWeb::sendChunk("{\"source\":\"flash\",\"start\":0,\"archive\":");
    sendUint32(archiveIndex);
    Esp32BaseWeb::sendChunk(",\"nextIndex\":0,\"limit\":");
    sendUint32(limitParam);
    Esp32BaseWeb::sendChunk(",\"count\":0,\"totalRecords\":0,\"recordBytes\":");
    sendUint32(kFeederRecordEncodedMaxBytes);
    Esp32BaseWeb::sendChunk(",\"records\":[]}");
    Esp32BaseWeb::endJson();
    return;
  }
#endif

  const FeederRecordSnapshot snapshot = g_records.snapshot();
  Esp32BaseWeb::sendChunk("{\"source\":\"ram\",\"start\":0,\"limit\":");
  sendUint32(kFeederRecentRecordCapacity);
  Esp32BaseWeb::sendChunk(",\"count\":");
  sendUint8(snapshot.count);
  Esp32BaseWeb::sendChunk(",\"totalRecords\":");
  sendUint8(snapshot.count);
  Esp32BaseWeb::sendChunk(",\"capacity\":");
  sendUint8(kFeederRecentRecordCapacity);
  Esp32BaseWeb::sendChunk(",\"records\":");
  sendRecordArray(snapshot);
  Esp32BaseWeb::sendChunk("}");
  Esp32BaseWeb::endJson();
#endif
}

void FarmFeederApp::handleFeederTarget() {
#if ESP32BASE_ENABLE_WEB
  uint8_t channel = 0;
  FeederTargetRequest request;
  if (!readUint8Param("channel", channel) || !validAppChannel(channel) ||
      !readTargetModeParam("targetMode", request.mode) ||
      !readInt32ParamOptional("targetGramsX100", request.targetGramsX100) ||
      !readInt32ParamOptional("targetRevolutionsX100", request.targetRevolutionsX100)) {
    sendResultJson(400, "InvalidArgument");
    return;
  }
  const FeederTargetResult result = g_targets.setTarget(channel, request);
  if (result == FeederTargetResult::Ok) {
    persistFeederTargetsIfReady();
  }
  sendResultJson(result == FeederTargetResult::Ok ? 200 : 400,
                 result == FeederTargetResult::Ok ? "Ok" : "InvalidArgument");
#endif
}

void FarmFeederApp::handleFeederManualStart() {
#if ESP32BASE_ENABLE_WEB
  uint8_t channelMask = 0;
  if (!readUint8Param("channelMask", channelMask) ||
      (channelMask & static_cast<uint8_t>(~configuredChannelMask())) != 0) {
    sendResultJson(400, "InvalidArgument");
    return;
  }
  FeederTargetSnapshot targetOverrides;
  if (!readManualTargetOverrides(channelMask, targetOverrides)) {
    sendResultJson(400, "InvalidArgument");
    return;
  }
  FeederTargetBatch targets;
  const uint32_t commandId = allocateCommandId();
  const FeederStartResult result =
      startResolvedTargets(channelMask, FeederRunSource::Manual, commandId, targets, &targetOverrides);
  FeederRecord record;
  record.commandId = commandId;
  record.type = FeederRecordType::ManualRequested;
  record.result = feederRecordResultFromCommand(result.result);
  record.requestedMask = channelMask;
  record.successMask = result.successMask;
  record.busyMask = result.busyMask;
  record.faultMask = result.faultMask;
  record.skippedMask = result.skippedMask;
  recordBusinessEvent(record);
  sendStartResultJson(result, &targets, commandId);
#endif
}

void FarmFeederApp::handleFeederStart() {
#if ESP32BASE_ENABLE_WEB
  uint8_t channelMask = 0;
  if (!readUint8Param("channelMask", channelMask)) {
    uint8_t channel = 0;
    if (!readUint8Param("channel", channel) || !validAppChannel(channel)) {
      sendResultJson(400, "InvalidArgument");
      return;
    }
    channelMask = static_cast<uint8_t>(1U << channel);
  } else if ((channelMask & static_cast<uint8_t>(~configuredChannelMask())) != 0) {
    sendResultJson(400, "InvalidArgument");
    return;
  }
  FeederTargetBatch targets;
  const uint32_t commandId = allocateCommandId();
  const FeederStartResult result =
      startResolvedTargets(channelMask, FeederRunSource::Manual, commandId, targets);
  FeederRecord record;
  record.commandId = commandId;
  record.type = FeederRecordType::ManualRequested;
  record.result = feederRecordResultFromCommand(result.result);
  record.requestedMask = channelMask;
  record.successMask = result.successMask;
  record.busyMask = result.busyMask;
  record.faultMask = result.faultMask;
  record.skippedMask = result.skippedMask;
  recordBusinessEvent(record);
  sendStartResultJson(result, &targets, commandId);
#endif
}

void FarmFeederApp::handleFeederStop() {
#if ESP32BASE_ENABLE_WEB
  uint8_t channelMask = 0;
  if (!readUint8Param("channelMask", channelMask)) {
    uint8_t channel = 0;
    if (!readUint8Param("channel", channel) || !validAppChannel(channel)) {
      sendResultJson(400, "InvalidArgument");
      return;
    }
    channelMask = static_cast<uint8_t>(1U << channel);
  } else if ((channelMask & static_cast<uint8_t>(~configuredChannelMask())) != 0) {
    sendResultJson(400, "InvalidArgument");
    return;
  }
  const uint32_t commandId = allocateCommandId();
  const FeederCommandResult result = g_feeder.stopChannels(channelMask);
  if (result == FeederCommandResult::Ok) {
    settleStoppedRuns(channelMask);
  }
  FeederRecord record;
  record.commandId = commandId;
  record.type = FeederRecordType::ChannelStopped;
  record.result = feederRecordResultFromCommand(result);
  record.requestedMask = channelMask;
  record.successMask = result == FeederCommandResult::Ok ? channelMask : 0;
  recordBusinessEvent(record);
  sendResultJson(result == FeederCommandResult::Ok ? 200 : 400,
                 feederCommandResultName(result),
                 commandId);
#endif
}

void FarmFeederApp::handleFeederStopAll() {
#if ESP32BASE_ENABLE_WEB
  const uint8_t runningMask = g_feeder.snapshot().runningChannelMask;
  const uint32_t commandId = allocateCommandId();
  const FeederCommandResult result = g_feeder.stopAll();
  if (result == FeederCommandResult::Ok) {
    settleStoppedRuns(runningMask);
  }
  FeederRecord record;
  record.commandId = commandId;
  record.type = FeederRecordType::ChannelStopped;
  record.result = feederRecordResultFromCommand(result);
  record.requestedMask = runningMask;
  record.successMask = result == FeederCommandResult::Ok ? runningMask : 0;
  recordBusinessEvent(record);
  sendResultJson(result == FeederCommandResult::Ok ? 200 : 400,
                 feederCommandResultName(result),
                 commandId);
#endif
}

void FarmFeederApp::handleMaintenanceClearToday() {
#if ESP32BASE_ENABLE_WEB
  const FeederSnapshot snapshot = g_feeder.snapshot();
  if (snapshot.runningChannelMask != 0) {
    sendResultJson(409, "Busy");
    return;
  }
  char resource[40];
  snprintf(resource, sizeof(resource), "today:%lu",
           static_cast<unsigned long>(g_schedules.snapshot().serviceDate));
  if (!requireConfirm("clear-today", resource)) {
    return;
  }

  const uint32_t commandId = allocateCommandId();
  g_schedules.clearToday();
  g_today.beginDay(g_schedules.snapshot().serviceDate);
  persistFeederScheduleIfReady();
  persistFeederTodayIfReady();

  FeederRecord record;
  record.commandId = commandId;
  record.type = FeederRecordType::TodayCleared;
  record.result = FeederRecordResult::Ok;
  recordBusinessEvent(record);

  sendResultJson(200, "Ok", commandId);
#endif
}

void FarmFeederApp::handleMaintenanceClearFault() {
#if ESP32BASE_ENABLE_WEB
  uint8_t channelMask = 0;
  if (!readUint8Param("channelMask", channelMask)) {
    uint8_t channel = 0;
    if (!readUint8Param("channel", channel) || !validAppChannel(channel)) {
      sendResultJson(400, "InvalidArgument");
      return;
    }
    channelMask = static_cast<uint8_t>(1U << channel);
  } else if ((channelMask & static_cast<uint8_t>(~configuredChannelMask())) != 0) {
    sendResultJson(400, "InvalidArgument");
    return;
  }
  char resource[40];
  snprintf(resource, sizeof(resource), "channelMask:%u", static_cast<unsigned>(channelMask));
  if (!requireConfirm("clear-fault", resource)) {
    return;
  }

  const uint32_t commandId = allocateCommandId();
  uint8_t successMask = 0;
  uint8_t skippedMask = 0;
  for (uint8_t i = 0; i < kFeederConfiguredChannels; ++i) {
    const uint8_t bit = static_cast<uint8_t>(1U << i);
    if ((channelMask & bit) == 0) {
      continue;
    }
    const FeederCommandResult result = g_feeder.clearChannelFault(i);
    if (result == FeederCommandResult::Ok) {
      successMask |= bit;
    } else {
      skippedMask |= bit;
    }
  }

  FeederRecord record;
  record.commandId = commandId;
  record.type = FeederRecordType::FaultCleared;
  record.result = successMask != 0 && skippedMask == 0
                    ? FeederRecordResult::Ok
                    : (successMask != 0 ? FeederRecordResult::Partial
                                        : FeederRecordResult::InvalidArgument);
  record.requestedMask = channelMask;
  record.successMask = successMask;
  record.skippedMask = skippedMask;
  recordBusinessEvent(record);

  Esp32BaseWeb::beginJson(successMask != 0 ? 200 : 400);
  Esp32BaseWeb::sendChunk("{\"result\":\"");
  Esp32BaseWeb::sendChunk(successMask != 0 && skippedMask == 0
                            ? "Ok"
                            : (successMask != 0 ? "Partial" : "InvalidArgument"));
  Esp32BaseWeb::sendChunk("\",\"commandId\":");
  sendUint32(commandId);
  Esp32BaseWeb::sendChunk(",\"requestedMask\":");
  sendUint8(channelMask);
  Esp32BaseWeb::sendChunk(",\"successMask\":");
  sendUint8(successMask);
  Esp32BaseWeb::sendChunk(",\"skippedMask\":");
  sendUint8(skippedMask);
  Esp32BaseWeb::sendChunk(",\"motorOutput\":{\"enabled\":false}}");
  Esp32BaseWeb::endJson();
#endif
}

void FarmFeederApp::handleScheduleCreate() {
#if ESP32BASE_ENABLE_WEB
  FeederPlanConfig config;
  if (!readPlanConfigFromParams(config)) {
    sendResultJson(400, "InvalidArgument");
    return;
  }
  const FeederScheduleMutation mutation = g_schedules.addPlan(config);
  if (mutation.result == FeederScheduleResult::Ok) {
    persistFeederScheduleIfReady();
  }
  Esp32BaseWeb::beginJson(mutation.result == FeederScheduleResult::Ok ? 200 : 400);
  Esp32BaseWeb::sendChunk("{\"result\":\"");
  Esp32BaseWeb::sendChunk(scheduleResultName(mutation.result));
  Esp32BaseWeb::sendChunk("\",\"planId\":");
  sendUint8(mutation.planId);
  Esp32BaseWeb::sendChunk("}");
  Esp32BaseWeb::endJson();
#endif
}

void FarmFeederApp::handleScheduleUpdate() {
#if ESP32BASE_ENABLE_WEB
  uint8_t planId = 0;
  FeederPlanConfig config;
  if (!readUint8Param("planId", planId) || !readPlanConfigFromParams(config)) {
    sendResultJson(400, "InvalidArgument");
    return;
  }
  const FeederScheduleResult result = g_schedules.updatePlan(planId, config);
  if (result == FeederScheduleResult::Ok) {
    persistFeederScheduleIfReady();
  }
  sendResultJson(result == FeederScheduleResult::Ok ? 200 : 400, scheduleResultName(result));
#endif
}

void FarmFeederApp::handleScheduleDelete() {
#if ESP32BASE_ENABLE_WEB
  uint8_t planId = 0;
  if (!readUint8Param("planId", planId)) {
    sendResultJson(400, "InvalidArgument");
    return;
  }
  char resource[40];
  snprintf(resource, sizeof(resource), "plan:%u", static_cast<unsigned>(planId));
  if (!requireConfirm("delete-plan", resource)) {
    return;
  }
  const FeederScheduleResult result = g_schedules.deletePlan(planId);
  if (result == FeederScheduleResult::Ok) {
    persistFeederScheduleIfReady();
  }
  sendResultJson(result == FeederScheduleResult::Ok ? 200 : 404, scheduleResultName(result));
#endif
}

void FarmFeederApp::handleScheduleSkip() {
#if ESP32BASE_ENABLE_WEB
  uint8_t planId = 0;
  uint32_t serviceDate = g_schedules.snapshot().serviceDate;
  if (!readUint8Param("planId", planId) || !readUint32ParamOptional("date", serviceDate) ||
      serviceDate == 0) {
    sendResultJson(400, "InvalidArgument");
    return;
  }
  char resource[40];
  snprintf(resource, sizeof(resource), "plan:%u:%lu",
           static_cast<unsigned>(planId),
           static_cast<unsigned long>(serviceDate));
  if (!requireConfirm("skip-occurrence", resource)) {
    return;
  }
  const FeederScheduleResult result = g_schedules.skipOccurrence(planId, serviceDate);
  if (result == FeederScheduleResult::Ok) {
    persistFeederScheduleIfReady();
  }
  sendResultJson(result == FeederScheduleResult::Ok ? 200 : 404, scheduleResultName(result));
#endif
}

void FarmFeederApp::handleScheduleCancelSkip() {
#if ESP32BASE_ENABLE_WEB
  uint8_t planId = 0;
  uint32_t serviceDate = g_schedules.snapshot().serviceDate;
  if (!readUint8Param("planId", planId) || !readUint32ParamOptional("date", serviceDate) ||
      serviceDate == 0) {
    sendResultJson(400, "InvalidArgument");
    return;
  }
  char resource[40];
  snprintf(resource, sizeof(resource), "plan:%u:%lu",
           static_cast<unsigned>(planId),
           static_cast<unsigned long>(serviceDate));
  if (!requireConfirm("cancel-skip", resource)) {
    return;
  }
  const FeederScheduleResult result = g_schedules.cancelSkipOccurrence(planId, serviceDate);
  if (result == FeederScheduleResult::Ok) {
    persistFeederScheduleIfReady();
  }
  sendResultJson(result == FeederScheduleResult::Ok ? 200 : 404, scheduleResultName(result));
#endif
}

void FarmFeederApp::handleBucketSetRemaining() {
#if ESP32BASE_ENABLE_WEB
  uint8_t channel = 0;
  int32_t remainGramsX100 = 0;
  if (!readUint8Param("channel", channel) || !validAppChannel(channel) ||
      !readInt32Param("remainGramsX100", remainGramsX100)) {
    sendResultJson(400, "InvalidArgument");
    return;
  }
  char resource[40];
  snprintf(resource, sizeof(resource), "bucket:%u", static_cast<unsigned>(channel));
  if (!requireConfirm("set-bucket", resource)) {
    return;
  }
  const FeederBucketResult result = g_buckets.setRemaining(channel, remainGramsX100, 0);
  if (result == FeederBucketResult::Ok) {
    persistFeederBucketsIfReady();
  }
  sendResultJson(result == FeederBucketResult::Ok ? 200 : 400, bucketResultName(result));
#endif
}

void FarmFeederApp::handleBucketAddFeed() {
#if ESP32BASE_ENABLE_WEB
  uint8_t channel = 0;
  int32_t addedGramsX100 = 0;
  if (!readUint8Param("channel", channel) || !validAppChannel(channel) ||
      !readInt32Param("addedGramsX100", addedGramsX100)) {
    sendResultJson(400, "InvalidArgument");
    return;
  }
  char resource[40];
  snprintf(resource, sizeof(resource), "bucket:%u", static_cast<unsigned>(channel));
  if (!requireConfirm("add-feed", resource)) {
    return;
  }
  const FeederBucketResult result = g_buckets.addFeed(channel, addedGramsX100, 0);
  if (result == FeederBucketResult::Ok) {
    persistFeederBucketsIfReady();
  }
  sendResultJson(result == FeederBucketResult::Ok ? 200 : 400, bucketResultName(result));
#endif
}

void FarmFeederApp::handleBucketMarkFull() {
#if ESP32BASE_ENABLE_WEB
  uint8_t channel = 0;
  if (!readUint8Param("channel", channel) || !validAppChannel(channel)) {
    sendResultJson(400, "InvalidArgument");
    return;
  }
  char resource[40];
  snprintf(resource, sizeof(resource), "bucket:%u", static_cast<unsigned>(channel));
  if (!requireConfirm("mark-full", resource)) {
    return;
  }
  const FeederBucketResult result = g_buckets.markFull(channel, 0);
  if (result == FeederBucketResult::Ok) {
    persistFeederBucketsIfReady();
  }
  sendResultJson(result == FeederBucketResult::Ok ? 200 : 400, bucketResultName(result));
#endif
}

void FarmFeederApp::handleBaseInfoChannel() {
#if ESP32BASE_ENABLE_WEB
  uint8_t channel = 0;
  bool enabled = false;
  int32_t outputPulsesPerRev = 0;
  int32_t gramsPerRevX100 = 0;
  int32_t capacityGramsX100 = 0;
  if (!readUint8Param("channel", channel) || !validAppChannel(channel) ||
      !readBoolParam("enabled", enabled) ||
      !readInt32Param("outputPulsesPerRev", outputPulsesPerRev) ||
      !readInt32Param("gramsPerRevX100", gramsPerRevX100) ||
      !readInt32Param("capacityGramsX100", capacityGramsX100)) {
    sendResultJson(400, "InvalidArgument");
    return;
  }
  if (g_feeder.snapshot().runningChannelMask != 0) {
    sendResultJson(400, "Busy");
    return;
  }
  char resource[40];
  snprintf(resource, sizeof(resource), "base-info:%u", static_cast<unsigned>(channel));
  if (!requireConfirm("base-info", resource)) {
    return;
  }

  FeederChannelBaseInfo info;
  info.enabled = enabled;
  info.outputPulsesPerRev = outputPulsesPerRev;
  info.gramsPerRevX100 = gramsPerRevX100;
  info.capacityGramsX100 = capacityGramsX100;
  const FeederBucketResult result = g_buckets.updateBaseInfo(channel, info);
  if (result == FeederBucketResult::Ok) {
    syncFeederConfigFromBaseInfo();
    persistFeederCalibrationIfReady();
  }
  sendResultJson(result == FeederBucketResult::Ok ? 200 : 400, bucketResultName(result));
#endif
}
