#include "FarmFeederApp.h"

#include <Arduino.h>
#include <Esp32Base.h>

#include <time.h>

#include "FeederBucket.h"
#include "FeederController.h"
#include "FeederRecordFileStore.h"
#include "FeederRecordLog.h"
#include "FeederRunTracker.h"
#include "FeederSchedule.h"
#include "FeederTarget.h"

namespace {

FeederController g_feeder;
FeederControllerConfig g_feederConfig;
FeederScheduleService g_schedules;
FeederBucketService g_buckets;
FeederTargetService g_targets;
FeederRunTracker g_runs;
FeederRecordLog g_records;
uint16_t g_lastScheduleMinute = 24 * 60;
uint32_t g_lastScheduleDate = 0;
bool g_recordStorageReady = false;

static constexpr uint8_t kFarmFeederApiRouteCount = 20;
static constexpr const char* kFeederRecordRootDir = "/records";
static constexpr const char* kFeederRecordDir = "/records/feeder";
static constexpr const char* kFeederRecordCurrentPath = "/records/feeder/current.far";
static_assert(ESP32BASE_WEB_MAX_ROUTES >= kFarmFeederApiRouteCount,
              "Esp32FarmFeeder requires ESP32BASE_WEB_MAX_ROUTES >= 20");

void addFarmFeederApi(const char* path, Esp32BaseWeb::Handler handler) {
  if (!Esp32BaseWeb::addApi(path, handler)) {
    ESP32BASE_LOG_E("farmfeeder", "api_route_register_failed path=%s", path);
  }
}

void syncFeederConfigFromBaseInfo() {
  g_feederConfig.enabledChannelMask =
      static_cast<uint8_t>(g_buckets.enabledChannelMask() & g_feederConfig.installedChannelMask);
  const FeederCommandResult result = g_feeder.configure(g_feederConfig);
  g_runs.stopAll();
  if (result != FeederCommandResult::Ok) {
    ESP32BASE_LOG_E("farmfeeder", "feeder_config_sync_failed result=%u",
                    static_cast<unsigned>(result));
  }
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
  }
  return "UnknownEvent";
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

void recordBusinessEvent(const FeederRecord& record) {
  const FeederRecord stored = g_records.append(record, currentRecordTime());
#if ESP32BASE_ENABLE_FS
  if (!ensureRecordStorageReady()) {
    return;
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
  for (uint8_t i = 0; i < kFeederMaxChannels; ++i) {
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

void sendBucketSummary(const FeederBucketSnapshot& snapshot) {
  Esp32BaseWeb::sendChunk("[");
  for (uint8_t i = 0; i < kFeederMaxChannels; ++i) {
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

void sendBaseInfoSummary(const FeederBucketSnapshot& snapshot) {
  Esp32BaseWeb::sendChunk("[");
  for (uint8_t i = 0; i < kFeederMaxChannels; ++i) {
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
  for (uint8_t i = 0; i < kFeederMaxChannels; ++i) {
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
  for (uint8_t i = 0; i < kFeederMaxChannels; ++i) {
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

void sendRecordArray(const FeederRecordSnapshot& snapshot) {
  Esp32BaseWeb::sendChunk("[");
  for (uint8_t i = 0; i < snapshot.count; ++i) {
    if (i > 0) {
      Esp32BaseWeb::sendChunk(",");
    }
    const FeederRecord& record = snapshot.records[i];
    char number[16];
    Esp32BaseWeb::sendChunk("{\"sequence\":");
    snprintf(number, sizeof(number), "%lu", static_cast<unsigned long>(record.sequence));
    Esp32BaseWeb::sendChunk(number);
    Esp32BaseWeb::sendChunk(",\"unixTime\":");
    snprintf(number, sizeof(number), "%lu", static_cast<unsigned long>(record.unixTime));
    Esp32BaseWeb::sendChunk(number);
    Esp32BaseWeb::sendChunk(",\"uptimeSec\":");
    snprintf(number, sizeof(number), "%lu", static_cast<unsigned long>(record.uptimeSec));
    Esp32BaseWeb::sendChunk(number);
    Esp32BaseWeb::sendChunk(",\"bootId\":");
    snprintf(number, sizeof(number), "%lu", static_cast<unsigned long>(record.bootId));
    Esp32BaseWeb::sendChunk(number);
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
  Esp32BaseWeb::sendChunk("]");
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

bool readTargetModeParam(const char* name, FeederTargetMode& out) {
  char raw[16];
  if (!Esp32BaseWeb::getParam(name, raw, sizeof(raw))) {
    out = FeederTargetMode::None;
    return true;
  }
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

const char* bucketResultName(FeederBucketResult result) {
  switch (result) {
    case FeederBucketResult::Ok: return "Ok";
    case FeederBucketResult::InvalidArgument: return "InvalidArgument";
    case FeederBucketResult::Underflow: return "Underflow";
  }
  return "InvalidArgument";
}

void sendResultJson(int code, const char* result) {
  Esp32BaseWeb::beginJson(code);
  Esp32BaseWeb::sendChunk("{\"result\":\"");
  Esp32BaseWeb::sendChunk(result);
  Esp32BaseWeb::sendChunk("\"}");
  Esp32BaseWeb::endJson();
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

void sendStartResultJson(const FeederStartResult& result, const FeederTargetBatch* targets = nullptr) {
  const bool success = result.result == FeederCommandResult::Ok ||
                       result.result == FeederCommandResult::Partial;
  Esp32BaseWeb::beginJson(success ? 200 : 400);
  Esp32BaseWeb::sendChunk("{\"result\":\"");
  Esp32BaseWeb::sendChunk(feederCommandResultName(result.result));
  Esp32BaseWeb::sendChunk("\",\"successMask\":");
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

FeederStartResult startResolvedTargets(uint8_t channelMask, FeederRunSource source,
                                       FeederTargetBatch& targets) {
  targets = resolveFeederTargetsForMask(g_buckets.snapshot(), g_targets.snapshot(), channelMask);
  if (targets.okMask == 0) {
    FeederStartResult result;
    result.result = FeederCommandResult::InvalidArgument;
    result.skippedMask = channelMask;
    return result;
  }
  FeederStartResult result = g_feeder.startChannels(targets.okMask, source);
  if (result.successMask != 0) {
    g_runs.start(result.successMask, source, targets);
    for (uint8_t i = 0; i < kFeederMaxChannels; ++i) {
      const uint8_t bit = static_cast<uint8_t>(1U << i);
      if ((result.successMask & bit) == 0) {
        continue;
      }
      FeederRecord record;
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

FeederTargetSnapshot targetSnapshotFromPlan(const FeederPlanConfig& plan) {
  FeederTargetSnapshot targets;
  for (uint8_t i = 0; i < kFeederMaxChannels; ++i) {
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

bool localDateAndMinute(uint32_t epochSec, uint32_t& date, uint16_t& minute) {
  time_t raw = static_cast<time_t>(epochSec);
  tm value = {};
  if (!localtime_r(&raw, &value)) {
    return false;
  }
  date = static_cast<uint32_t>((value.tm_year + 1900) * 10000 +
                               (value.tm_mon + 1) * 100 +
                               value.tm_mday);
  minute = static_cast<uint16_t>(value.tm_hour * 60 + value.tm_min);
  return true;
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

  for (uint8_t i = 0; i < kFeederMaxChannels; ++i) {
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
  configureStaticDefaults();

  Esp32Base::setFirmwareInfo("Esp32FarmFeeder", "0.1.0");
  configureAppConfigPage();
  configureBusinessShell();

  if (!Esp32Base::begin()) {
    ESP32BASE_LOG_E("farmfeeder", "Esp32Base begin failed: %s", Esp32Base::lastError());
  } else {
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
  if (!localDateAndMinute(timeSnapshot.epochSec, serviceDate, currentMinute)) {
    return;
  }

  if (serviceDate != g_lastScheduleDate) {
    g_schedules.beginDay(serviceDate);
    g_lastScheduleDate = serviceDate;
    g_lastScheduleMinute = 24 * 60;
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
  const FeederStartResult result = startPlanTargets(plan, targets);
  FeederRecord record;
  record.type = FeederRecordType::ScheduleTriggered;
  record.result = feederRecordResultFromCommand(result.result);
  record.planId = tick.planId;
  record.requestedMask = plan.channelMask;
  record.successMask = result.successMask;
  record.busyMask = result.busyMask;
  record.faultMask = result.faultMask;
  record.skippedMask = result.skippedMask;
  recordBusinessEvent(record);
  ESP32BASE_LOG_I("farmfeeder",
                  "schedule_triggered plan_id=%u result=%u success_mask=%u skipped_mask=%u",
                  static_cast<unsigned>(tick.planId),
                  static_cast<unsigned>(result.result),
                  static_cast<unsigned>(result.successMask),
                  static_cast<unsigned>(result.skippedMask));
#endif
}

void FarmFeederApp::configureStaticDefaults() {
  g_feederConfig.installedChannelMask = 0b0111;
  g_feederConfig.enabledChannelMask = 0b0111;

  g_schedules.beginDay(0);

  FeederChannelBaseInfo channelInfo;
  channelInfo.enabled = true;
  channelInfo.outputPulsesPerRev = 4320;
  channelInfo.gramsPerRevX100 = 7000;
  channelInfo.capacityGramsX100 = 500000;
  for (uint8_t i = 0; i < 3; ++i) {
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
  Esp32BaseAppConfig::addGroup({"channels", "通道"});
  Esp32BaseAppConfig::addGroup({"schedule", "计划"});

  Esp32BaseAppConfig::addBool({"channels", "feeder", "channel1Enabled", "通道 1 启用", true,
                               "业务通道启用状态。", false, nullptr});
  Esp32BaseAppConfig::addBool({"channels", "feeder", "channel2Enabled", "通道 2 启用", true,
                               "业务通道启用状态。", false, nullptr});
  Esp32BaseAppConfig::addBool({"channels", "feeder", "channel3Enabled", "通道 3 启用", true,
                               "业务通道启用状态。", false, nullptr});
  Esp32BaseAppConfig::addInt({"schedule", "feeder", "startIntervalMs", "顺序启动间隔", 1000, 0,
                              10000, 100, "ms", "多通道启动时降低浪涌。", false, nullptr});
#endif
}

void FarmFeederApp::configureBusinessShell() {
#if ESP32BASE_ENABLE_WEB
  Esp32BaseWeb::setDeviceName("Esp32FarmFeeder");
  Esp32BaseWeb::setHomeMode(Esp32BaseWeb::HOME_ESP32BASE);
  Esp32BaseWeb::setSystemNavMode(Esp32BaseWeb::SYSTEM_NAV_BOTTOM);
  addFarmFeederApi("/api/app/status", FarmFeederApp::sendStatusJson);
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
  Esp32BaseWeb::sendChunk(",\"motorOutput\":{\"enabled\":false}}");
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
  const FeederRecordSnapshot snapshot = g_records.snapshot();
  Esp32BaseWeb::beginJson(200);
  Esp32BaseWeb::sendChunk("{\"count\":");
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
  if (!readUint8Param("channel", channel) || !readTargetModeParam("targetMode", request.mode) ||
      !readInt32ParamOptional("targetGramsX100", request.targetGramsX100) ||
      !readInt32ParamOptional("targetRevolutionsX100", request.targetRevolutionsX100)) {
    sendResultJson(400, "InvalidArgument");
    return;
  }
  const FeederTargetResult result = g_targets.setTarget(channel, request);
  sendResultJson(result == FeederTargetResult::Ok ? 200 : 400,
                 result == FeederTargetResult::Ok ? "Ok" : "InvalidArgument");
#endif
}

void FarmFeederApp::handleFeederManualStart() {
#if ESP32BASE_ENABLE_WEB
  uint8_t channelMask = 0;
  if (!readUint8Param("channelMask", channelMask)) {
    sendResultJson(400, "InvalidArgument");
    return;
  }
  FeederTargetBatch targets;
  const FeederStartResult result = startResolvedTargets(channelMask, FeederRunSource::Manual, targets);
  FeederRecord record;
  record.type = FeederRecordType::ManualRequested;
  record.result = feederRecordResultFromCommand(result.result);
  record.requestedMask = channelMask;
  record.successMask = result.successMask;
  record.busyMask = result.busyMask;
  record.faultMask = result.faultMask;
  record.skippedMask = result.skippedMask;
  recordBusinessEvent(record);
  sendStartResultJson(result, &targets);
#endif
}

void FarmFeederApp::handleFeederStart() {
#if ESP32BASE_ENABLE_WEB
  uint8_t channelMask = 0;
  if (!readUint8Param("channelMask", channelMask)) {
    uint8_t channel = 0;
    if (!readUint8Param("channel", channel) || channel >= kFeederMaxChannels) {
      sendResultJson(400, "InvalidArgument");
      return;
    }
    channelMask = static_cast<uint8_t>(1U << channel);
  }
  FeederTargetBatch targets;
  const FeederStartResult result = startResolvedTargets(channelMask, FeederRunSource::Manual, targets);
  FeederRecord record;
  record.type = FeederRecordType::ManualRequested;
  record.result = feederRecordResultFromCommand(result.result);
  record.requestedMask = channelMask;
  record.successMask = result.successMask;
  record.busyMask = result.busyMask;
  record.faultMask = result.faultMask;
  record.skippedMask = result.skippedMask;
  recordBusinessEvent(record);
  sendStartResultJson(result, &targets);
#endif
}

void FarmFeederApp::handleFeederStop() {
#if ESP32BASE_ENABLE_WEB
  uint8_t channelMask = 0;
  if (!readUint8Param("channelMask", channelMask)) {
    uint8_t channel = 0;
    if (!readUint8Param("channel", channel) || channel >= kFeederMaxChannels) {
      sendResultJson(400, "InvalidArgument");
      return;
    }
    channelMask = static_cast<uint8_t>(1U << channel);
  }
  const FeederCommandResult result = g_feeder.stopChannels(channelMask);
  if (result == FeederCommandResult::Ok) {
    g_runs.stop(channelMask);
  }
  FeederRecord record;
  record.type = FeederRecordType::ChannelStopped;
  record.result = feederRecordResultFromCommand(result);
  record.requestedMask = channelMask;
  record.successMask = result == FeederCommandResult::Ok ? channelMask : 0;
  recordBusinessEvent(record);
  sendResultJson(result == FeederCommandResult::Ok ? 200 : 400, feederCommandResultName(result));
#endif
}

void FarmFeederApp::handleFeederStopAll() {
#if ESP32BASE_ENABLE_WEB
  const uint8_t runningMask = g_feeder.snapshot().runningChannelMask;
  const FeederCommandResult result = g_feeder.stopAll();
  if (result == FeederCommandResult::Ok) {
    g_runs.stopAll();
  }
  FeederRecord record;
  record.type = FeederRecordType::ChannelStopped;
  record.result = feederRecordResultFromCommand(result);
  record.requestedMask = runningMask;
  record.successMask = result == FeederCommandResult::Ok ? runningMask : 0;
  recordBusinessEvent(record);
  sendResultJson(result == FeederCommandResult::Ok ? 200 : 400, feederCommandResultName(result));
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
  const FeederScheduleResult result = g_schedules.deletePlan(planId);
  sendResultJson(result == FeederScheduleResult::Ok ? 200 : 404, scheduleResultName(result));
#endif
}

void FarmFeederApp::handleScheduleSkip() {
#if ESP32BASE_ENABLE_WEB
  uint8_t planId = 0;
  if (!readUint8Param("planId", planId)) {
    sendResultJson(400, "InvalidArgument");
    return;
  }
  const FeederScheduleResult result = g_schedules.skipToday(planId);
  sendResultJson(result == FeederScheduleResult::Ok ? 200 : 404, scheduleResultName(result));
#endif
}

void FarmFeederApp::handleScheduleCancelSkip() {
#if ESP32BASE_ENABLE_WEB
  uint8_t planId = 0;
  if (!readUint8Param("planId", planId)) {
    sendResultJson(400, "InvalidArgument");
    return;
  }
  const FeederScheduleResult result = g_schedules.cancelSkipToday(planId);
  sendResultJson(result == FeederScheduleResult::Ok ? 200 : 404, scheduleResultName(result));
#endif
}

void FarmFeederApp::handleBucketSetRemaining() {
#if ESP32BASE_ENABLE_WEB
  uint8_t channel = 0;
  int32_t remainGramsX100 = 0;
  if (!readUint8Param("channel", channel) || !readInt32Param("remainGramsX100", remainGramsX100)) {
    sendResultJson(400, "InvalidArgument");
    return;
  }
  const FeederBucketResult result = g_buckets.setRemaining(channel, remainGramsX100, 0);
  sendResultJson(result == FeederBucketResult::Ok ? 200 : 400, bucketResultName(result));
#endif
}

void FarmFeederApp::handleBucketAddFeed() {
#if ESP32BASE_ENABLE_WEB
  uint8_t channel = 0;
  int32_t addedGramsX100 = 0;
  if (!readUint8Param("channel", channel) || !readInt32Param("addedGramsX100", addedGramsX100)) {
    sendResultJson(400, "InvalidArgument");
    return;
  }
  const FeederBucketResult result = g_buckets.addFeed(channel, addedGramsX100, 0);
  sendResultJson(result == FeederBucketResult::Ok ? 200 : 400, bucketResultName(result));
#endif
}

void FarmFeederApp::handleBucketMarkFull() {
#if ESP32BASE_ENABLE_WEB
  uint8_t channel = 0;
  if (!readUint8Param("channel", channel)) {
    sendResultJson(400, "InvalidArgument");
    return;
  }
  const FeederBucketResult result = g_buckets.markFull(channel, 0);
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
  if (!readUint8Param("channel", channel) || !readBoolParam("enabled", enabled) ||
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

  FeederChannelBaseInfo info;
  info.enabled = enabled;
  info.outputPulsesPerRev = outputPulsesPerRev;
  info.gramsPerRevX100 = gramsPerRevX100;
  info.capacityGramsX100 = capacityGramsX100;
  const FeederBucketResult result = g_buckets.updateBaseInfo(channel, info);
  if (result == FeederBucketResult::Ok) {
    syncFeederConfigFromBaseInfo();
  }
  sendResultJson(result == FeederBucketResult::Ok ? 200 : 400, bucketResultName(result));
#endif
}
