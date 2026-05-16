#include "FarmFeederApp.h"

#include <Arduino.h>
#include <Esp32Base.h>

#include "FeederBucket.h"
#include "FeederController.h"
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

static constexpr uint8_t kFarmFeederApiRouteCount = 19;
static_assert(ESP32BASE_WEB_MAX_ROUTES >= kFarmFeederApiRouteCount,
              "Esp32FarmFeeder requires ESP32BASE_WEB_MAX_ROUTES >= 19");

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

void sendUint8(uint8_t value) {
  char number[8];
  snprintf(number, sizeof(number), "%u", static_cast<unsigned>(value));
  Esp32BaseWeb::sendChunk(number);
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
  }
  result.skippedMask |= static_cast<uint8_t>(targets.invalidMask | targets.notCalibratedMask);
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
  sendResultJson(result == FeederCommandResult::Ok ? 200 : 400, feederCommandResultName(result));
#endif
}

void FarmFeederApp::handleFeederStopAll() {
#if ESP32BASE_ENABLE_WEB
  const FeederCommandResult result = g_feeder.stopAll();
  if (result == FeederCommandResult::Ok) {
    g_runs.stopAll();
  }
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
