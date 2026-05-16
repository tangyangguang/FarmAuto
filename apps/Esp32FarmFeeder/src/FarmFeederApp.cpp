#include "FarmFeederApp.h"

#include <Arduino.h>
#include <Esp32Base.h>

#include "FeederController.h"
#include "FeederSchedule.h"

namespace {

FeederController g_feeder;
FeederControllerConfig g_feederConfig;
FeederScheduleService g_schedules;

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

void sendUint8(uint8_t value) {
  char number[8];
  snprintf(number, sizeof(number), "%u", static_cast<unsigned>(value));
  Esp32BaseWeb::sendChunk(number);
}

void sendChannelArray(const FeederSnapshot& snapshot) {
  Esp32BaseWeb::sendChunk("[");
  for (uint8_t i = 0; i < kFeederMaxChannels; ++i) {
    if (i > 0) {
      Esp32BaseWeb::sendChunk(",");
    }
    Esp32BaseWeb::sendChunk("{\"index\":");
    sendUint8(i);
    Esp32BaseWeb::sendChunk(",\"state\":\"");
    Esp32BaseWeb::sendChunk(channelStateName(snapshot.channels[i]));
    Esp32BaseWeb::sendChunk("\"}");
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
  const FeederCommandResult result = g_feeder.configure(g_feederConfig);
  if (result != FeederCommandResult::Ok) {
    ESP32BASE_LOG_E("farmfeeder", "feeder_config_failed result=%u", static_cast<unsigned>(result));
  }

  g_schedules.beginDay(0);
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
  Esp32BaseWeb::addApi("/api/app/status", FarmFeederApp::sendStatusJson);
#endif
}

void FarmFeederApp::sendStatusJson() {
#if ESP32BASE_ENABLE_WEB
  const FeederSnapshot snapshot = g_feeder.snapshot();
  const FeederScheduleSnapshot scheduleSnapshot = g_schedules.snapshot();

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
  sendChannelArray(snapshot);
  Esp32BaseWeb::sendChunk(",\"schedule\":");
  sendScheduleSummary(scheduleSnapshot);
  Esp32BaseWeb::sendChunk(",\"motorOutput\":{\"enabled\":false}}");
  Esp32BaseWeb::endJson();
#endif
}
