#include "FarmDoorApp.h"

#include <Arduino.h>
#include <Esp32At24cRecordStore.h>
#include <Esp32Base.h>
#include <Esp32EncodedDcMotor.h>
#include <FarmAutoEventLog.h>
#include <Esp32MotorCurrentGuard.h>

#include <esp_system.h>
#include <cstdlib>
#include <cstring>

#include "DoorController.h"
#include "DoorRecordFileStore.h"
#include "DoorRecordLog.h"
#include "DoorRecoveryApply.h"
#include "DoorRecoveryStore.h"
#include "DoorStorageLayout.h"
#include "FarmDoorHardware.h"

namespace {

constexpr int64_t kDefaultOutputPulsesPerRev = 2096;
constexpr int64_t kDefaultOpenTurnsX100 = 500;
constexpr int64_t kDefaultOpenTargetPulses =
    (kDefaultOutputPulsesPerRev * kDefaultOpenTurnsX100) / 100;
constexpr int64_t kDefaultMaxRunPulses = (kDefaultOpenTargetPulses * 150) / 100;

DoorController g_door;
DoorRecordLog g_records;
uint32_t g_nextCommandId = 1;
bool g_recordStorageReady = false;
DoorControllerConfig g_doorConfig;
Esp32EncodedDcMotor::MotorHardwareConfig g_motorHardware;
Esp32EncodedDcMotor::EncoderBackendConfig g_encoderBackend;
Esp32EncodedDcMotor::MotorKinematics g_motorKinematics;
Esp32EncodedDcMotor::MotorMotionProfile g_motorProfile;
Esp32EncodedDcMotor::MotorProtection g_motorProtection;
Esp32EncodedDcMotor::MotorStopPolicy g_motorStopPolicy;
Esp32EncodedDcMotor::At8236HBridgeDriver g_motorDriver;
Esp32EncodedDcMotor::PcntEncoderReader g_motorEncoder;
Esp32EncodedDcMotor::EncodedDcMotor g_motor;
Esp32MotorCurrentGuard::Ina240A2AnalogConfig g_currentSensor;
Esp32MotorCurrentGuard::MotorCurrentGuardConfig g_currentGuard;
Esp32At24cRecordStore::RecordStoreConfig g_recordStoreConfig;
Esp32At24cRecordStore::ArduinoWireI2cBus g_at24cBus;
Esp32At24cRecordStore::At24cI2cDevice g_at24cDevice(g_at24cBus, {});
Esp32At24cRecordStore::RecordStore g_at24cStore;
bool g_at24cStoreReady = false;
bool g_motorOutputEnabled = false;
bool g_motorOutputReady = false;

static constexpr uint8_t kFarmDoorRouteCount = 16;
static_assert(ESP32BASE_WEB_MAX_ROUTES >= kFarmDoorRouteCount,
              "Esp32FarmDoor requires ESP32BASE_WEB_MAX_ROUTES >= 16");
static constexpr const char* kDoorRecordRootDir = "/records";
static constexpr const char* kDoorRecordDir = "/records/door";
static constexpr const char* kDoorRecordCurrentPath = "/records/door/current.dar";
static constexpr uint32_t kDoorRecordMaxCurrentBytes = 64UL * 1024UL;
static constexpr uint8_t kDoorRecordMaxArchives = 16;

bool persistDoorRecoveryStateIfReady();

bool ina240CompileEnabled() {
#if FARMAUTO_FARMDOOR_ENABLE_INA240A2
  return true;
#else
  return false;
#endif
}

const char* boolJson(bool value) {
  return value ? "true" : "false";
}

void beginRawJson(int code) {
  Esp32BaseWeb::beginResponse(code, "application/json", nullptr);
}

void endRawJson() {
  Esp32BaseWeb::endResponse();
}

bool requireApiAuth() {
  if (Esp32BaseWeb::isMethod(Esp32BaseWeb::METHOD_POST)) {
    return Esp32BaseWeb::checkPostAllowed("farmdoor");
  }
  return Esp32BaseWeb::checkAuth();
}

void sendDigitalField(const char* name, uint8_t value) {
  Esp32BaseWeb::sendChunk("\"");
  Esp32BaseWeb::sendChunk(name);
  Esp32BaseWeb::sendChunk("\":");
  Esp32BaseWeb::sendChunk(value == 1 ? "1" : "0");
}

void sendInt64(int64_t value) {
  char number[24];
  snprintf(number, sizeof(number), "%lld", static_cast<long long>(value));
  Esp32BaseWeb::sendChunk(number);
}

void sendUint32(uint32_t value) {
  char number[16];
  snprintf(number, sizeof(number), "%lu", static_cast<unsigned long>(value));
  Esp32BaseWeb::sendChunk(number);
}

uint32_t allocateCommandId() {
  const uint32_t commandId = g_nextCommandId;
  ++g_nextCommandId;
  if (g_nextCommandId == 0) {
    g_nextCommandId = 1;
  }
  return commandId;
}

const char* stateName(DoorState state) {
  switch (state) {
    case DoorState::PositionUnknown: return "PositionUnknown";
    case DoorState::IdleClosed: return "IdleClosed";
    case DoorState::IdleOpen: return "IdleOpen";
    case DoorState::IdlePartial: return "IdlePartial";
    case DoorState::Opening: return "Opening";
    case DoorState::Closing: return "Closing";
    case DoorState::Fault: return "Fault";
  }
  return "Unknown";
}

const char* commandName(DoorCommand command) {
  switch (command) {
    case DoorCommand::None: return "None";
    case DoorCommand::Open: return "Open";
    case DoorCommand::Close: return "Close";
    case DoorCommand::Stop: return "Stop";
  }
  return "Unknown";
}

const char* trustName(PositionTrustLevel trustLevel) {
  switch (trustLevel) {
    case PositionTrustLevel::Trusted: return "Trusted";
    case PositionTrustLevel::Limited: return "Limited";
    case PositionTrustLevel::Untrusted: return "Untrusted";
  }
  return "Untrusted";
}

const char* stopReasonName(DoorStopReason reason) {
  switch (reason) {
    case DoorStopReason::None: return "None";
    case DoorStopReason::TargetReached: return "TargetReached";
    case DoorStopReason::UserStop: return "UserStop";
    case DoorStopReason::ProtectiveStop: return "ProtectiveStop";
    case DoorStopReason::FaultStop: return "FaultStop";
  }
  return "Unknown";
}

const char* faultReasonName(DoorFaultReason reason) {
  switch (reason) {
    case DoorFaultReason::None: return "None";
    case DoorFaultReason::InvalidConfig: return "InvalidConfig";
    case DoorFaultReason::MotorFault: return "MotorFault";
    case DoorFaultReason::PositionLost: return "PositionLost";
  }
  return "Unknown";
}

const char* commandResultName(DoorCommandResult result) {
  switch (result) {
    case DoorCommandResult::Ok: return "Ok";
    case DoorCommandResult::Busy: return "Busy";
    case DoorCommandResult::InvalidArgument: return "InvalidArgument";
    case DoorCommandResult::PositionUntrusted: return "PositionUntrusted";
    case DoorCommandResult::FaultActive: return "FaultActive";
  }
  return "InvalidArgument";
}

const char* recordTypeName(DoorRecordType type) {
  switch (type) {
    case DoorRecordType::CommandRequested: return "DoorCommandRequested";
    case DoorRecordType::PositionSet: return "DoorPositionSet";
    case DoorRecordType::TravelSet: return "DoorTravelSet";
    case DoorRecordType::TravelAdjusted: return "DoorTravelAdjusted";
    case DoorRecordType::FaultCleared: return "DoorFaultCleared";
  }
  return "UnknownEvent";
}

const char* recordResultName(DoorRecordResult result) {
  switch (result) {
    case DoorRecordResult::Ok: return "Ok";
    case DoorRecordResult::Busy: return "Busy";
    case DoorRecordResult::InvalidArgument: return "InvalidArgument";
    case DoorRecordResult::PositionUntrusted: return "PositionUntrusted";
    case DoorRecordResult::FaultActive: return "FaultActive";
  }
  return "InvalidArgument";
}

int httpCodeFor(DoorCommandResult result) {
  switch (result) {
    case DoorCommandResult::Ok: return 200;
    case DoorCommandResult::Busy: return 409;
    case DoorCommandResult::PositionUntrusted: return 409;
    case DoorCommandResult::FaultActive: return 409;
    case DoorCommandResult::InvalidArgument: return 400;
  }
  return 400;
}

uint16_t positionPercent(const DoorSnapshot& snapshot) {
  if (snapshot.openTargetPulses <= 0) {
    return 0;
  }
  int64_t percent = (snapshot.positionPulses * 100) / snapshot.openTargetPulses;
  if (percent < 0) {
    percent = 0;
  }
  if (percent > 120) {
    percent = 120;
  }
  return static_cast<uint16_t>(percent);
}

void formatInt64(char* out, std::size_t outSize, int64_t value) {
  if (out == nullptr || outSize == 0) {
    return;
  }
  snprintf(out, outSize, "%lld", static_cast<long long>(value));
}

void formatInt64WithUnit(char* out, std::size_t outSize, int64_t value, const char* unit) {
  if (out == nullptr || outSize == 0) {
    return;
  }
  snprintf(out,
           outSize,
           "%lld %s",
           static_cast<long long>(value),
           unit == nullptr ? "" : unit);
}

void formatPercent(char* out, std::size_t outSize, uint16_t value) {
  if (out == nullptr || outSize == 0) {
    return;
  }
  snprintf(out, outSize, "%u%%", static_cast<unsigned>(value));
}

bool readInt64Param(const char* name, int64_t& out) {
  char raw[24];
  if (!Esp32BaseWeb::getParam(name, raw, sizeof(raw))) {
    return false;
  }
  char* end = nullptr;
  const long long value = strtoll(raw, &end, 10);
  if (!end || *end != '\0') {
    return false;
  }
  out = static_cast<int64_t>(value);
  return true;
}

bool hasParam(const char* name) {
  return Esp32BaseWeb::hasParam(name);
}

bool readInt64ParamOptional(const char* name, int64_t& out) {
  char raw[24];
  if (!Esp32BaseWeb::getParam(name, raw, sizeof(raw))) {
    return true;
  }
  char* end = nullptr;
  const long long value = strtoll(raw, &end, 10);
  if (!end || *end != '\0') {
    return false;
  }
  out = static_cast<int64_t>(value);
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

struct PageParams {
  uint32_t page = 1;
  uint32_t per = 15;
};

bool readPageParams(uint32_t defaultPer, uint32_t maxPer, PageParams& out) {
  PageParams params;
  params.per = defaultPer;
  if (!readUint32ParamOptional("page", params.page) ||
      !readUint32ParamOptional("per", params.per) || params.page == 0 || params.per == 0) {
    return false;
  }
  if (params.per > maxPer) {
    params.per = maxPer;
  }
  out = params;
  return true;
}

bool pageStartIndex(const PageParams& params, uint32_t& out) {
  if (params.page == 0 || params.per == 0) {
    return false;
  }
  const uint32_t pageOffset = params.page - 1;
  if (pageOffset > UINT32_MAX / params.per) {
    return false;
  }
  out = pageOffset * params.per;
  return true;
}

void sendUint32Text(uint32_t value) {
  char text[16];
  snprintf(text, sizeof(text), "%lu", static_cast<unsigned long>(value));
  Esp32BaseWeb::sendChunk(text);
}

void sendInt64Text(int64_t value) {
  char text[24];
  snprintf(text, sizeof(text), "%lld", static_cast<long long>(value));
  Esp32BaseWeb::sendChunk(text);
}

void sendTimeText(uint32_t unixTime, uint32_t uptimeSec) {
  if (unixTime > 0) {
    Esp32BaseWeb::sendChunk("Unix ");
    sendUint32Text(unixTime);
    return;
  }
  Esp32BaseWeb::sendChunk("启动 +");
  sendUint32Text(uptimeSec);
  Esp32BaseWeb::sendChunk(" 秒");
}

void sendRecordPulseRange(int64_t oldValue, int64_t newValue) {
  sendInt64Text(oldValue);
  Esp32BaseWeb::sendChunk(" -> ");
  sendInt64Text(newValue);
}

void sendRecordDelta(int64_t oldValue, int64_t newValue, int64_t deltaValue) {
  sendRecordPulseRange(oldValue, newValue);
  Esp32BaseWeb::sendChunk(" (");
  if (deltaValue > 0) {
    Esp32BaseWeb::sendChunk("+");
  }
  sendInt64Text(deltaValue);
  Esp32BaseWeb::sendChunk(")");
}

void sendPlainRow(const char* label, const char* value, const char* detail = nullptr) {
  Esp32BaseWeb::sendChunk("<tr><td class='muted'>");
  Esp32BaseWeb::writeHtmlEscaped(label ? label : "");
  Esp32BaseWeb::sendChunk("</td><td>");
  Esp32BaseWeb::writeHtmlEscaped(value ? value : "");
  if (detail != nullptr && detail[0] != '\0') {
    Esp32BaseWeb::sendChunk("<small>");
    Esp32BaseWeb::writeHtmlEscaped(detail);
    Esp32BaseWeb::sendChunk("</small>");
  }
  Esp32BaseWeb::sendChunk("</td></tr>");
}

void beginPlainTable() {
  Esp32BaseWeb::sendChunk("<div class='tablewrap'><table class='part'>");
}

void endPlainTable() {
  Esp32BaseWeb::sendChunk("</table></div>");
}

void sendDoorStatusTable(const DoorSnapshot& snapshot,
                         const char* positionValue,
                         const char* targetValue,
                         const char* percentValue,
                         const char* maxRunValue) {
  char stateDetail[48];
  snprintf(stateDetail, sizeof(stateDetail), "当前命令：%s", commandName(snapshot.activeCommand));
  char positionDetail[48];
  snprintf(positionDetail, sizeof(positionDetail), "可信度：%s", trustName(snapshot.positionTrustLevel));
  char targetDetail[48];
  snprintf(targetDetail, sizeof(targetDetail), "保护边界：%s", maxRunValue);
  char motorDetail[48];
  snprintf(motorDetail,
           sizeof(motorDetail),
           "%s / %s",
           g_motorOutputEnabled ? "输出已启用" : "输出未启用",
           g_motorOutputReady ? "ready" : "not ready");

  beginPlainTable();
  sendPlainRow("门状态", stateName(snapshot.state), stateDetail);
  sendPlainRow("门位判断", percentValue, positionDetail);
  sendPlainRow("当前位置", positionValue, "编码器累计位置");
  sendPlainRow("开门目标", targetValue, targetDetail);
  sendPlainRow("最近停止", stopReasonName(snapshot.lastStopReason), "用于判断上次停止是否正常");
  sendPlainRow("电机输出", g_motorOutputEnabled ? "已启用" : "未启用", motorDetail);
  sendPlainRow("故障", faultReasonName(snapshot.faultReason),
               snapshot.faultReason == DoorFaultReason::None ? "当前无业务故障" : "需要处理后再运行");
  endPlainTable();
}

void sendDoorActionButtons(const DoorSnapshot& snapshot) {
  Esp32BaseWeb::sendChunk("<div class='actions'>");
  Esp32BaseWeb::sendChunk("<form method='post' action='/api/app/door/open' onsubmit='return once(this)'><input class='btnlink ok' type='submit' value='开门'></form>");
  Esp32BaseWeb::sendChunk("<form method='post' action='/api/app/door/close' onsubmit='return once(this)'><input class='btnlink info' type='submit' value='关门'></form>");
  Esp32BaseWeb::sendChunk("<form method='post' action='/api/app/door/stop' onsubmit='return once(this)'><input class='btnlink danger' type='submit' value='停止'></form>");
  if (snapshot.state == DoorState::Fault || snapshot.faultReason != DoorFaultReason::None) {
    Esp32BaseWeb::sendChunk("<form method='post' action='/api/app/maintenance/clear-fault' onsubmit=\"return confirm('清除当前故障？')&&once(this)\"><input class='btnlink danger' type='submit' value='清除故障'></form>");
  }
  Esp32BaseWeb::sendChunk("</div>");
}

void sendCalibrationStatusTable(const DoorSnapshot& snapshot,
                                const char* positionValue,
                                const char* targetValue,
                                const char* turnsValue,
                                const char* pulsesPerRevValue) {
  beginPlainTable();
  sendPlainRow("当前位置", positionValue, trustName(snapshot.positionTrustLevel));
  sendPlainRow("开门目标", targetValue, turnsValue);
  sendPlainRow("每圈信号数", pulsesPerRevValue, "output pulses/rev");
  sendPlainRow("最近停止", stopReasonName(snapshot.lastStopReason), "校准后建议确认停止原因");
  endPlainTable();
}

void sendDiagnosticsStatusTable(const FarmDoorReadOnlyDiagnostics& diagnostics,
                                const char* at24cValue,
                                const char* currentValue) {
  char currentDetail[80];
  snprintf(currentDetail,
           sizeof(currentDetail),
           "min=%d max=%d avg=%d samples=%u",
           diagnostics.currentRawMin,
           diagnostics.currentRawMax,
           diagnostics.currentRawAvg,
           diagnostics.currentSampleCount);
  char buttonValue[48];
  snprintf(buttonValue,
           sizeof(buttonValue),
           "开:%u 关:%u 停:%u 辅:%u",
           diagnostics.buttonOpen,
           diagnostics.buttonClose,
           diagnostics.buttonStop,
           diagnostics.buttonAux);
  char encoderValue[32];
  snprintf(encoderValue, sizeof(encoderValue), "A:%u B:%u", diagnostics.encoderA, diagnostics.encoderB);
  char motorValue[48];
  snprintf(motorValue,
           sizeof(motorValue),
           "%s / %s",
           g_motorOutputEnabled ? "输出已启用" : "输出未启用",
           g_motorOutputReady ? "ready" : "not ready");

  beginPlainTable();
  sendPlainRow("AT24C128", at24cValue, g_at24cStoreReady ? "记录存储已就绪" : "记录存储未就绪");
  sendPlainRow("电流采样", currentValue, currentDetail);
  sendPlainRow("电流保护", g_currentGuard.enabled ? "已启用" : "未启用",
               ina240CompileEnabled() ? "固件已包含 INA240A2 支持" : "固件未启用 INA240A2");
  sendPlainRow("电机输出", g_motorOutputEnabled ? "已启用" : "未启用", motorValue);
  sendPlainRow("按钮输入", buttonValue, "1 表示高电平，0 表示低电平");
  sendPlainRow("编码器输入", encoderValue, "只读 GPIO 当前电平");
  endPlainTable();
}

int64_t openTargetPulsesFromTurnsX100(int64_t turnsX100) {
  return (kDefaultOutputPulsesPerRev * turnsX100) / 100;
}

int64_t turnsX100FromOpenTargetPulses(int64_t openTargetPulses) {
  return (openTargetPulses * 100) / kDefaultOutputPulsesPerRev;
}

int64_t defaultMaxRunPulsesForTarget(int64_t openTargetPulses) {
  return (openTargetPulses * 150) / 100;
}

bool readTrustLevelParam(PositionTrustLevel& out) {
  char raw[16];
  if (!Esp32BaseWeb::getParam("trustLevel", raw, sizeof(raw))) {
    out = PositionTrustLevel::Trusted;
    return true;
  }
  if (strcmp(raw, "Trusted") == 0 || strcmp(raw, "trusted") == 0) {
    out = PositionTrustLevel::Trusted;
    return true;
  }
  if (strcmp(raw, "Limited") == 0 || strcmp(raw, "limited") == 0) {
    out = PositionTrustLevel::Limited;
    return true;
  }
  if (strcmp(raw, "Untrusted") == 0 || strcmp(raw, "untrusted") == 0) {
    out = PositionTrustLevel::Untrusted;
    return true;
  }
  return false;
}

void applyRuntimeConfigFromBase() {
#if ESP32BASE_ENABLE_APP_CONFIG
  const int32_t speedPercent = Esp32BaseConfig::getInt("door", "speed", 60);
  const int32_t openTurnsX100 = Esp32BaseConfig::getInt("door", "openTurns", 500);
  const int32_t maxRunMs = Esp32BaseConfig::getInt("door", "maxRunMs", 25000);
  g_motorOutputEnabled = Esp32BaseConfig::getBool("door", "motorOutput", false);
  const bool requestedCurrentGuard =
      Esp32BaseConfig::getBool("door", "currentGuard", false);

  if (speedPercent >= 10 && speedPercent <= 100) {
    g_motorProfile.speedPercent = static_cast<uint8_t>(speedPercent);
  }
  if (openTurnsX100 > 0) {
    g_doorConfig.openTargetPulses = openTargetPulsesFromTurnsX100(openTurnsX100);
    g_doorConfig.maxRunPulses = defaultMaxRunPulsesForTarget(g_doorConfig.openTargetPulses);
    g_motorProtection.maxRunPulses = g_doorConfig.maxRunPulses;
  }
  if (maxRunMs > 0) {
    g_doorConfig.maxRunMs = static_cast<uint32_t>(maxRunMs);
    g_motorProtection.maxRunMs = g_doorConfig.maxRunMs;
  }
  const DoorSnapshot snapshot = g_door.snapshot();
  if (snapshot.state != DoorState::Opening && snapshot.state != DoorState::Closing) {
    g_door.configure(g_doorConfig);
  }
  if (g_motorOutputReady) {
    g_motor.configure(g_motorKinematics, g_motorProfile, g_motorProtection, g_motorStopPolicy);
  }
  g_currentGuard.enabled = ina240CompileEnabled() && requestedCurrentGuard;
#else
  g_motorOutputEnabled = false;
  g_currentGuard.enabled = false;
#endif
}

bool configureDoorMotorIfEnabled() {
  if (!g_motorOutputEnabled) {
    g_motorOutputReady = false;
    return false;
  }
  if (g_motorOutputReady) {
    return true;
  }
  Esp32EncodedDcMotor::MotorResult result = g_motorEncoder.begin(g_encoderBackend);
  if (result != Esp32EncodedDcMotor::MotorResult::Ok) {
    ESP32BASE_LOG_E("farmdoor", "motor_encoder_begin_failed result=%u",
                    static_cast<unsigned>(result));
    FarmAutoEventLog::recordStorageWarning("door_motor", "encoder_begin_failed",
                                           static_cast<uint16_t>(result));
    return false;
  }
  g_motorEncoder.resetPosition(g_door.snapshot().positionPulses);
  result = g_motorDriver.begin(FarmDoorHw.pins().motorIn1, FarmDoorHw.pins().motorIn2,
                               g_motorHardware);
  if (result != Esp32EncodedDcMotor::MotorResult::Ok) {
    ESP32BASE_LOG_E("farmdoor", "motor_driver_begin_failed result=%u",
                    static_cast<unsigned>(result));
    FarmAutoEventLog::recordStorageWarning("door_motor", "driver_begin_failed",
                                           static_cast<uint16_t>(result));
    return false;
  }
  result = g_motor.begin(g_motorDriver, g_motorEncoder, g_motorHardware, g_encoderBackend);
  if (result == Esp32EncodedDcMotor::MotorResult::Ok) {
    result = g_motor.configure(g_motorKinematics, g_motorProfile, g_motorProtection,
                               g_motorStopPolicy);
  }
  if (result != Esp32EncodedDcMotor::MotorResult::Ok) {
    ESP32BASE_LOG_E("farmdoor", "motor_begin_failed result=%u", static_cast<unsigned>(result));
    FarmAutoEventLog::recordStorageWarning("door_motor", "motor_begin_failed",
                                           static_cast<uint16_t>(result));
    return false;
  }
  g_motorOutputReady = true;
  return true;
}

DoorCommandResult startDoorMotorTo(int64_t targetPulses) {
  if (!configureDoorMotorIfEnabled()) {
    return DoorCommandResult::InvalidArgument;
  }
  const Esp32EncodedDcMotor::MotorResult result = g_motor.requestMoveToPosition(targetPulses);
  if (result == Esp32EncodedDcMotor::MotorResult::Ok) {
    return DoorCommandResult::Ok;
  }
  ESP32BASE_LOG_W("farmdoor", "motor_start_failed result=%u", static_cast<unsigned>(result));
  g_door.enterFault(DoorFaultReason::MotorFault);
  FarmAutoEventLog::recordDoorProtectionStopped("motor_start_failed", g_door.snapshot().positionPulses);
  return DoorCommandResult::FaultActive;
}

void sendMotorOutputJson() {
  Esp32BaseWeb::sendChunk("\"motorOutput\":{\"enabled\":");
  Esp32BaseWeb::sendChunk(boolJson(g_motorOutputEnabled));
  Esp32BaseWeb::sendChunk(",\"ready\":");
  Esp32BaseWeb::sendChunk(boolJson(g_motorOutputReady));
  if (g_motorOutputReady) {
    const Esp32EncodedDcMotor::MotorSnapshot motor = g_motor.snapshot();
    Esp32BaseWeb::sendChunk(",\"positionPulses\":");
    sendInt64(motor.positionPulses);
    Esp32BaseWeb::sendChunk(",\"targetPulses\":");
    sendInt64(motor.targetPulses);
    Esp32BaseWeb::sendChunk(",\"outputPercent\":");
    sendUint32(motor.driverOutputPercent);
    Esp32BaseWeb::sendChunk(",\"faultReason\":");
    sendUint32(static_cast<uint32_t>(motor.faultReason));
  }
  Esp32BaseWeb::sendChunk("}");
}

void updateDoorMotorRuntime() {
  if (!g_motorOutputReady) {
    return;
  }
  g_motor.update(millis());
  const DoorSnapshot door = g_door.snapshot();
  if (door.state != DoorState::Opening && door.state != DoorState::Closing) {
    return;
  }
  const Esp32EncodedDcMotor::MotorSnapshot motor = g_motor.snapshot();
  if (motor.state == Esp32EncodedDcMotor::MotorState::Idle) {
    if (g_door.onMotionTargetReached(motor.positionPulses) == DoorCommandResult::Ok) {
      persistDoorRecoveryStateIfReady();
    }
    return;
  }
  if (motor.state == Esp32EncodedDcMotor::MotorState::Fault) {
    g_door.enterFault(DoorFaultReason::MotorFault);
    FarmAutoEventLog::recordDoorProtectionStopped("motor_fault", motor.positionPulses);
    persistDoorRecoveryStateIfReady();
  }
}

DoorRecordTime currentRecordTime() {
  DoorRecordTime time;
#if ESP32BASE_ENABLE_NTP
  const Esp32BaseNtp::TimeSnapshot snapshot = Esp32BaseNtp::snapshot();
  time.unixTime = snapshot.synced ? snapshot.epochSec : 0;
  time.uptimeSec = snapshot.uptimeSec;
  time.bootId = snapshot.bootId;
#endif
  return time;
}

#if ESP32BASE_ENABLE_FS
bool appendDoorRecordBytes(const char* path, const uint8_t* data, std::size_t length, void*) {
  return Esp32BaseFs::appendBytes(path, data, length);
}

bool doorRecordPathExists(const char* path, void*) {
  return Esp32BaseFs::exists(path);
}

int64_t doorRecordFileSize(const char* path, void*) {
  return Esp32BaseFs::fileSize(path);
}

bool removeDoorRecordFile(const char* path, void*) {
  return Esp32BaseFs::removeFile(path);
}

bool renameDoorRecordFile(const char* from, const char* to, void*) {
  return Esp32BaseFs::rename(from, to);
}

bool readDoorRecordBytesAt(const char* path,
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
  if (!Esp32BaseFs::exists(kDoorRecordRootDir) && !Esp32BaseFs::mkdir(kDoorRecordRootDir)) {
    return false;
  }
  if (!Esp32BaseFs::exists(kDoorRecordDir) && !Esp32BaseFs::mkdir(kDoorRecordDir)) {
    return false;
  }
  g_recordStorageReady = true;
  return true;
}
#endif

bool doorRecordPathForArchive(uint32_t archiveIndex, char* out, std::size_t outSize) {
  if (out == nullptr || outSize == 0 || archiveIndex > kDoorRecordMaxArchives) {
    return false;
  }
  const int written = archiveIndex == 0
                        ? snprintf(out, outSize, "%s", kDoorRecordCurrentPath)
                        : snprintf(out,
                                   outSize,
                                   "%s.%lu",
                                   kDoorRecordCurrentPath,
                                   static_cast<unsigned long>(archiveIndex));
  return written > 0 && static_cast<std::size_t>(written) < outSize;
}

void recordBusinessEvent(const DoorRecord& record) {
  const DoorRecord stored = g_records.append(record, currentRecordTime());
#if ESP32BASE_ENABLE_FS
  if (!ensureRecordStorageReady()) {
    return;
  }
  const DoorRecordRotateResult rotateResult =
      rotateDoorRecordPathIfNeeded(kDoorRecordCurrentPath,
                                   kDoorRecordMaxCurrentBytes,
                                   kDoorRecordMaxArchives,
                                   kDoorRecordEncodedMaxBytes,
                                   doorRecordFileSize,
                                   doorRecordPathExists,
                                   removeDoorRecordFile,
                                   renameDoorRecordFile,
                                   nullptr);
  if (rotateResult != DoorRecordRotateResult::Ok) {
    ESP32BASE_LOG_W("farmdoor", "record_rotate_failed result=%u",
                    static_cast<unsigned>(rotateResult));
    FarmAutoEventLog::recordStorageWarning("door_records",
                                           "rotate_failed",
                                           static_cast<uint16_t>(rotateResult));
  }
  const DoorRecordWriteResult result = appendDoorRecordToPath(
      stored, kDoorRecordCurrentPath, appendDoorRecordBytes, nullptr);
  if (result != DoorRecordWriteResult::Ok) {
    ESP32BASE_LOG_W("farmdoor", "record_append_failed result=%u",
                    static_cast<unsigned>(result));
    FarmAutoEventLog::recordStorageWarning("door_records",
                                           "append_failed",
                                           static_cast<uint16_t>(result));
  }
#endif
}

void sendRecordJson(const DoorRecord& record) {
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
  Esp32BaseWeb::sendChunk("\",\"command\":\"");
  Esp32BaseWeb::sendChunk(commandName(record.command));
  Esp32BaseWeb::sendChunk("\",\"oldPositionPulses\":");
  sendInt64(record.oldPositionPulses);
  Esp32BaseWeb::sendChunk(",\"newPositionPulses\":");
  sendInt64(record.newPositionPulses);
  Esp32BaseWeb::sendChunk(",\"oldTravelPulses\":");
  sendInt64(record.oldTravelPulses);
  Esp32BaseWeb::sendChunk(",\"newTravelPulses\":");
  sendInt64(record.newTravelPulses);
  Esp32BaseWeb::sendChunk(",\"deltaPulses\":");
  sendInt64(record.deltaPulses);
  Esp32BaseWeb::sendChunk("}");
}

void sendRecordHtmlRow(const DoorRecord& record) {
  Esp32BaseWeb::sendChunk("<tr><td>");
  sendUint32Text(record.sequence);
  Esp32BaseWeb::sendChunk("</td><td>");
  sendTimeText(record.unixTime, record.uptimeSec);
  Esp32BaseWeb::sendChunk("</td><td>");
  Esp32BaseWeb::writeHtmlEscaped(recordTypeName(record.type));
  Esp32BaseWeb::sendChunk("</td><td>");
  Esp32BaseWeb::writeHtmlEscaped(commandName(record.command));
  Esp32BaseWeb::sendChunk("</td><td>");
  Esp32BaseWeb::writeHtmlEscaped(recordResultName(record.result));
  Esp32BaseWeb::sendChunk("</td><td>");
  sendRecordPulseRange(record.oldPositionPulses, record.newPositionPulses);
  Esp32BaseWeb::sendChunk("</td><td>");
  sendRecordDelta(record.oldTravelPulses, record.newTravelPulses, record.deltaPulses);
  Esp32BaseWeb::sendChunk("</td></tr>");
}

void sendRecordSnapshotJson(const char* source) {
  const DoorRecordSnapshot snapshot = g_records.snapshot();
  beginRawJson(200);
  Esp32BaseWeb::sendChunk("{\"source\":\"");
  Esp32BaseWeb::sendChunk(source);
  Esp32BaseWeb::sendChunk("\",\"count\":");
  sendUint32(snapshot.count);
  Esp32BaseWeb::sendChunk(",\"records\":[");
  for (uint8_t i = 0; i < snapshot.count; ++i) {
    if (i > 0) {
      Esp32BaseWeb::sendChunk(",");
    }
    sendRecordJson(snapshot.records[i]);
  }
  Esp32BaseWeb::sendChunk("]}");
  endRawJson();
}

const char* appEventLevelName(FarmAutoEventLog::Level level) {
  switch (level) {
    case FarmAutoEventLog::LEVEL_INFO: return "info";
    case FarmAutoEventLog::LEVEL_WARN: return "warn";
    case FarmAutoEventLog::LEVEL_ERROR: return "error";
  }
  return "info";
}

void sendBusinessEventJson(const FarmAutoEventLog::BusinessEvent& event) {
  Esp32BaseWeb::sendChunk("{\"id\":");
  sendUint32(event.id);
  Esp32BaseWeb::sendChunk(",\"epochSec\":");
  sendUint32(event.epochSec);
  Esp32BaseWeb::sendChunk(",\"bootId\":");
  sendUint32(event.bootId);
  Esp32BaseWeb::sendChunk(",\"uptimeSec\":");
  sendUint32(event.uptimeSec);
  Esp32BaseWeb::sendChunk(",\"timeSynced\":");
  Esp32BaseWeb::sendChunk(boolJson(event.timeSynced));
  Esp32BaseWeb::sendChunk(",\"level\":\"");
  Esp32BaseWeb::sendChunk(appEventLevelName(event.level));
  Esp32BaseWeb::sendChunk("\",\"domain\":\"");
  Esp32BaseWeb::writeJsonEscaped(event.domain);
  Esp32BaseWeb::sendChunk("\",\"action\":\"");
  Esp32BaseWeb::writeJsonEscaped(event.action);
  Esp32BaseWeb::sendChunk("\",\"target\":\"");
  Esp32BaseWeb::writeJsonEscaped(event.target);
  Esp32BaseWeb::sendChunk("\",\"message\":\"");
  Esp32BaseWeb::writeJsonEscaped(event.message);
  Esp32BaseWeb::sendChunk("\",\"detail\":\"");
  Esp32BaseWeb::writeJsonEscaped(event.detail);
  Esp32BaseWeb::sendChunk("\",\"code\":");
  sendUint32(event.code);
  Esp32BaseWeb::sendChunk(",\"valueMask\":");
  sendUint32(event.valueMask);
  Esp32BaseWeb::sendChunk(",\"value1\":");
  sendInt64(event.value1);
  Esp32BaseWeb::sendChunk(",\"value2\":");
  sendInt64(event.value2);
  Esp32BaseWeb::sendChunk(",\"value3\":");
  sendInt64(event.value3);
  Esp32BaseWeb::sendChunk("}");
}

struct BusinessEventJsonState {
  uint16_t emitted = 0;
};

void sendBusinessEventJsonCallback(const FarmAutoEventLog::BusinessEvent& event, void* user) {
  BusinessEventJsonState* state = static_cast<BusinessEventJsonState*>(user);
  if (state == nullptr) {
    return;
  }
  if (state->emitted > 0) {
    Esp32BaseWeb::sendChunk(",");
  }
  sendBusinessEventJson(event);
  ++state->emitted;
}

struct BusinessEventTableState {
  uint16_t emitted = 0;
};

void sendBusinessEventHtmlRow(const FarmAutoEventLog::BusinessEvent& event) {
  Esp32BaseWeb::sendChunk("<tr><td>");
  sendUint32Text(event.id);
  Esp32BaseWeb::sendChunk("</td><td>");
  sendTimeText(event.timeSynced ? event.epochSec : 0, event.uptimeSec);
  Esp32BaseWeb::sendChunk("</td><td>");
  Esp32BaseWeb::writeHtmlEscaped(appEventLevelName(event.level));
  Esp32BaseWeb::sendChunk("</td><td>");
  Esp32BaseWeb::writeHtmlEscaped(event.domain);
  Esp32BaseWeb::sendChunk("</td><td>");
  Esp32BaseWeb::writeHtmlEscaped(event.action);
  Esp32BaseWeb::sendChunk("</td><td>");
  Esp32BaseWeb::writeHtmlEscaped(event.target);
  Esp32BaseWeb::sendChunk("</td><td>");
  Esp32BaseWeb::writeHtmlEscaped(event.message);
  Esp32BaseWeb::sendChunk("</td><td>");
  Esp32BaseWeb::writeHtmlEscaped(event.detail);
  Esp32BaseWeb::sendChunk("</td></tr>");
}

void sendBusinessEventHtmlCallback(const FarmAutoEventLog::BusinessEvent& event, void* user) {
  BusinessEventTableState* state = static_cast<BusinessEventTableState*>(user);
  if (state == nullptr) {
    return;
  }
  sendBusinessEventHtmlRow(event);
  ++state->emitted;
}

void sendRecentCommandJson() {
  const DoorRecordSnapshot records = g_records.snapshot();
  for (uint8_t i = records.count; i > 0; --i) {
    const DoorRecord& record = records.records[i - 1];
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
    Esp32BaseWeb::sendChunk("\",\"command\":\"");
    Esp32BaseWeb::sendChunk(commandName(record.command));
    Esp32BaseWeb::sendChunk("\",\"result\":\"");
    Esp32BaseWeb::sendChunk(recordResultName(record.result));
    Esp32BaseWeb::sendChunk("\"}");
    return;
  }
  Esp32BaseWeb::sendChunk("null");
}

void sendCommandResultJson(DoorCommandResult result, uint32_t commandId = 0) {
#if ESP32BASE_ENABLE_WEB
  const DoorSnapshot snapshot = g_door.snapshot();
  beginRawJson(httpCodeFor(result));
  Esp32BaseWeb::sendChunk("{\"result\":\"");
  Esp32BaseWeb::sendChunk(commandResultName(result));
  Esp32BaseWeb::sendChunk("\",\"commandId\":");
  sendUint32(commandId);
  Esp32BaseWeb::sendChunk(",\"state\":\"");
  Esp32BaseWeb::sendChunk(stateName(snapshot.state));
  Esp32BaseWeb::sendChunk("\",\"activeCommand\":\"");
  Esp32BaseWeb::sendChunk(commandName(snapshot.activeCommand));
  Esp32BaseWeb::sendChunk("\",\"positionPulses\":");
  sendInt64(snapshot.positionPulses);
  Esp32BaseWeb::sendChunk(",\"targetPulses\":");
  sendInt64(snapshot.targetPulses);
  Esp32BaseWeb::sendChunk(",");
  sendMotorOutputJson();
  Esp32BaseWeb::sendChunk("}");
  endRawJson();
#endif
}

void sendTravelResultJson(DoorCommandResult result, bool configSaved, uint32_t commandId = 0) {
#if ESP32BASE_ENABLE_WEB
  const DoorSnapshot snapshot = g_door.snapshot();
  beginRawJson(httpCodeFor(result));
  Esp32BaseWeb::sendChunk("{\"result\":\"");
  Esp32BaseWeb::sendChunk(commandResultName(result));
  Esp32BaseWeb::sendChunk("\",\"commandId\":");
  sendUint32(commandId);
  Esp32BaseWeb::sendChunk(",\"state\":\"");
  Esp32BaseWeb::sendChunk(stateName(snapshot.state));
  Esp32BaseWeb::sendChunk("\",\"openTargetPulses\":");
  sendInt64(snapshot.openTargetPulses);
  Esp32BaseWeb::sendChunk(",\"openTurnsX100\":");
  sendInt64(turnsX100FromOpenTargetPulses(snapshot.openTargetPulses));
  Esp32BaseWeb::sendChunk(",\"maxRunPulses\":");
  sendInt64(g_doorConfig.maxRunPulses);
  Esp32BaseWeb::sendChunk(",\"configSaved\":");
  Esp32BaseWeb::sendChunk(configSaved ? "true" : "false");
  Esp32BaseWeb::sendChunk(",");
  sendMotorOutputJson();
  Esp32BaseWeb::sendChunk("}");
  endRawJson();
#endif
}

DoorCommandResult applyTravelUpdate(int64_t openTargetPulses,
                                    int64_t maxRunPulses,
                                    bool& configSaved) {
  configSaved = false;
  if (openTargetPulses <= g_doorConfig.closedPositionPulses || maxRunPulses <= 0) {
    return DoorCommandResult::InvalidArgument;
  }

  const DoorControllerConfig oldDoorConfig = g_doorConfig;
  const int64_t oldProtectionMaxRunPulses = g_motorProtection.maxRunPulses;
  g_doorConfig.openTargetPulses = openTargetPulses;
  g_doorConfig.maxRunPulses = maxRunPulses;
  g_motorProtection.maxRunPulses = maxRunPulses;

  const DoorCommandResult result = g_door.updateTravel(openTargetPulses, maxRunPulses);
  if (result != DoorCommandResult::Ok) {
    g_doorConfig = oldDoorConfig;
    g_motorProtection.maxRunPulses = oldProtectionMaxRunPulses;
    return result;
  }

#if ESP32BASE_ENABLE_APP_CONFIG
  const int64_t turnsX100 = turnsX100FromOpenTargetPulses(openTargetPulses);
  if (turnsX100 > 0 && turnsX100 <= 2000) {
    configSaved = Esp32BaseConfig::setInt("door", "openTurns", static_cast<int32_t>(turnsX100));
  }
#endif
  return result;
}

#if ESP32BASE_ENABLE_APP_CONFIG
void onAppConfigChange(const Esp32BaseAppConfig::Change& change) {
  if (change.field.ns && change.field.key && strcmp(change.field.ns, "door") == 0 &&
      strcmp(change.field.key, "currentGuard") == 0) {
    applyRuntimeConfigFromBase();
    ESP32BASE_LOG_I("farmdoor", "current_guard_runtime_enabled=%s",
                    g_currentGuard.enabled ? "yes" : "no");
  }
}
#endif

void applyRecoveredDoorConfig(const DoorRecoveryState& state) {
  g_doorConfig.closedPositionPulses = state.closedPositionPulses;
  g_doorConfig.openTargetPulses = state.openTargetPulses;
  g_doorConfig.maxRunPulses = state.maxRunPulses;
  g_doorConfig.maxRunMs = state.maxRunMs;
  g_motorProtection.maxRunPulses = state.maxRunPulses;
  g_motorProtection.maxRunMs = state.maxRunMs;
}

void initializeDoorAt24cStore() {
  const Esp32At24cRecordStore::Result beginResult =
      g_at24cStore.begin(g_at24cDevice, kDoorAt24cConfig, kDoorAt24cRegions, kDoorAt24cRegionCount);
  if (beginResult != Esp32At24cRecordStore::Result::Ok) {
    ESP32BASE_LOG_W("farmdoor", "at24c_store_begin_failed result=%u",
                    static_cast<unsigned>(beginResult));
    return;
  }
  g_at24cStoreReady = true;

  DoorRecoveryState recovered;
  const Esp32At24cRecordStore::Result loadResult = loadDoorRecoveryState(g_at24cStore, recovered);
  if (loadResult == Esp32At24cRecordStore::Result::Ok) {
    const DoorCommandResult applyResult = applyDoorRecoveryState(g_door, recovered);
    if (applyResult == DoorCommandResult::Ok) {
      applyRecoveredDoorConfig(recovered);
      ESP32BASE_LOG_I("farmdoor", "door_recovery_restored position=%lld target=%lld",
                      static_cast<long long>(recovered.positionPulses),
                      static_cast<long long>(recovered.openTargetPulses));
    } else {
      ESP32BASE_LOG_W("farmdoor", "door_recovery_apply_failed result=%u",
                      static_cast<unsigned>(applyResult));
    }
    return;
  }
  if (loadResult != Esp32At24cRecordStore::Result::FormatRequired) {
    ESP32BASE_LOG_W("farmdoor", "door_recovery_load_failed result=%u",
                    static_cast<unsigned>(loadResult));
  }
}

bool persistDoorRecoveryStateIfReady() {
  if (!g_at24cStoreReady) {
    return false;
  }
  const DoorRecordTime time = currentRecordTime();
  const DoorRecoveryState state = doorRecoveryStateFromSnapshot(
      g_door.snapshot(), g_doorConfig, time.unixTime, time.uptimeSec, time.bootId);
  const Esp32At24cRecordStore::Result result = saveDoorRecoveryState(g_at24cStore, state);
  if (result == Esp32At24cRecordStore::Result::Ok ||
      result == Esp32At24cRecordStore::Result::Unchanged) {
    return true;
  }
  ESP32BASE_LOG_W("farmdoor", "door_recovery_save_failed result=%u",
                  static_cast<unsigned>(result));
  return false;
}

}  // namespace

FarmDoorApp FarmDoor;

void FarmDoorApp::begin() {
  Serial.begin(115200);
  configureStaticDefaults();
  configureHardwareInputs();

  Esp32Base::setFirmwareInfo("Esp32FarmDoor", "0.1.0");
  configureAppConfigPage();
  configureBusinessShell();

  if (!Esp32Base::begin()) {
    ESP32BASE_LOG_E("farmdoor", "Esp32Base begin failed: %s", Esp32Base::lastError());
  } else {
    initializeDoorAt24cStore();
    applyRuntimeConfigFromBase();
    configureDoorMotorIfEnabled();
    ESP32BASE_LOG_I("farmdoor", "skeleton ready, ina240a2_gpio=%u enabled=%s",
                    FarmDoorHw.pins().currentAdc,
                    g_currentGuard.enabled ? "yes" : "no");
  }
}

void FarmDoorApp::handle() {
  Esp32Base::handle();
  updateDoorMotorRuntime();
}

void FarmDoorApp::configureHardwareInputs() {
  FarmDoorHw.begin(FarmDoorHardwarePins{});
}

void FarmDoorApp::configureStaticDefaults() {
  g_motorHardware.driverType = Esp32EncodedDcMotor::DriverType::At8236HBridge;
  g_motorHardware.pwmFrequencyHz = 20000;
  g_motorHardware.pwmResolutionBits = 8;
  g_motorHardware.ledcChannelA = 0;
  g_motorHardware.ledcChannelB = 1;

  g_encoderBackend.backendType = Esp32EncodedDcMotor::EncoderBackendType::Pcnt;
  g_encoderBackend.pinA = FarmDoorHw.pins().encoderA;
  g_encoderBackend.pinB = FarmDoorHw.pins().encoderB;
  g_encoderBackend.countMode = Esp32EncodedDcMotor::CountMode::X1;
  g_encoderBackend.pcntUnit = 0;
  g_encoderBackend.glitchFilterNs = 1000;

  g_motorKinematics.motorShaftPulsesPerRev = 16;
  g_motorKinematics.gearRatio = 131.0f;
  g_motorKinematics.outputPulsesPerRev = kDefaultOutputPulsesPerRev;
  g_motorKinematics.countMode = Esp32EncodedDcMotor::CountMode::X1;

  g_motorProfile.speedPercent = 60;
  g_motorProfile.softStartMs = 1000;
  g_motorProfile.softStopMs = 500;
  g_motorProfile.minEffectiveSpeedPercent = 15;

  g_motorProtection.startupGraceMs = 1000;
  g_motorProtection.maxRunMs = 25000;
  g_motorProtection.maxRunPulses = kDefaultMaxRunPulses;

  g_motorStopPolicy.emergencyOutputMode = Esp32EncodedDcMotor::EmergencyOutputMode::Coast;

  g_currentSensor.adcPin = FarmDoorHw.pins().currentAdc;
  g_currentSensor.amplifierGain = 50.0f;
  g_currentSensor.senseResistorMilliOhm = 5.0f;
  g_currentSensor.bidirectional = true;

#if FARMAUTO_FARMDOOR_ENABLE_INA240A2
  g_currentGuard.enabled = false;  // 软件支持已接入；实测校准前默认不启用保护动作。
#else
  g_currentGuard.enabled = false;
#endif
  g_currentGuard.warningThresholdMa = 1800;
  g_currentGuard.faultThresholdMa = 2500;
  g_currentGuard.startupGraceMs = 1000;

  g_recordStoreConfig.layoutVersion = 1;
  g_recordStoreConfig.baseAddress = 0;
  g_recordStoreConfig.totalBytes = 16 * 1024;
  g_recordStoreConfig.pageSize = 64;

  g_doorConfig.closedPositionPulses = 0;
  g_doorConfig.openTargetPulses = kDefaultOpenTargetPulses;
  g_doorConfig.maxRunPulses = kDefaultMaxRunPulses;
  g_doorConfig.maxRunMs = g_motorProtection.maxRunMs;
  const DoorCommandResult doorConfigResult = g_door.configure(g_doorConfig);
  if (doorConfigResult != DoorCommandResult::Ok) {
    ESP32BASE_LOG_E("farmdoor", "door_config_failed result=%u", static_cast<unsigned>(doorConfigResult));
  }
}

void FarmDoorApp::configureAppConfigPage() {
#if ESP32BASE_ENABLE_APP_CONFIG
  Esp32BaseAppConfig::setTitle("Esp32FarmDoor 参数");
  Esp32BaseAppConfig::setChangeCallback(onAppConfigChange);
  Esp32BaseAppConfig::addGroup({"motor", "电机"});
  Esp32BaseAppConfig::addGroup({"protection", "保护"});
  Esp32BaseAppConfig::addGroup({"current", "电流检测"});

  Esp32BaseAppConfig::addInt({"motor", "door", "speed", "运行速度", 60, 10, 100, 5, "%",
                              "普通开门/关门速度。", false, nullptr});
  Esp32BaseAppConfig::addBool({"motor", "door", "motorOutput", "启用电机输出", false,
                               "启用后开关门命令会输出 AT8236 PWM。", true, nullptr});
  Esp32BaseAppConfig::addDecimal({"motor", "door", "openTurns", "开门目标", 500, 0, 2000, 5, 2,
                                  "圈", "开门目标圈数，可由校准流程更新。", false, nullptr});
  Esp32BaseAppConfig::addInt({"protection", "door", "maxRunMs", "最大运行时长", 25000, 1000,
                              120000, 1000, "ms", "保底停止边界。", false, nullptr});
  Esp32BaseAppConfig::addBool({"current", "door", "currentGuard", "启用 INA240A2", false,
                               "GPIO33 电流检测，实测稳定后启用。", false, nullptr});
#endif
}

void FarmDoorApp::configureBusinessShell() {
#if ESP32BASE_ENABLE_WEB
  Esp32BaseWeb::setDeviceName("Esp32FarmDoor");
  Esp32BaseWeb::setHomePath("/index");
  Esp32BaseWeb::setHomeMode(Esp32BaseWeb::HOME_APP);
  Esp32BaseWeb::setSystemNavMode(Esp32BaseWeb::SYSTEM_NAV_SECTION);
  Esp32BaseWeb::addNavItem("/index", "首页");
  Esp32BaseWeb::addNavItem("/records", "开关门记录");
  Esp32BaseWeb::addNavItem("/events", "业务事件");
  Esp32BaseWeb::addNavItem("/calibration", "校准");
  Esp32BaseWeb::addNavItem("/diagnostics", "诊断");
  Esp32BaseWeb::addPage("/index", "自动门首页", FarmDoorApp::sendHomePage);
  Esp32BaseWeb::addPage("/records", "自动门记录", FarmDoorApp::sendRecordsPage);
  Esp32BaseWeb::addPage("/events", "业务事件", FarmDoorApp::sendEventsPage);
  Esp32BaseWeb::addPage("/calibration", "行程校准", FarmDoorApp::sendCalibrationPage);
  Esp32BaseWeb::addPage("/diagnostics", "自动门诊断", FarmDoorApp::sendDiagnosticsPage);
  Esp32BaseWeb::addApi("/api/app/status", FarmDoorApp::sendStatusJson);
  Esp32BaseWeb::addApi("/api/app/diagnostics", FarmDoorApp::sendDiagnosticsJson);
  Esp32BaseWeb::addApi("/api/app/events/recent", FarmDoorApp::sendRecentEventsJson);
  Esp32BaseWeb::addApi("/api/app/records", FarmDoorApp::sendRecordsJson);
  Esp32BaseWeb::addApi("/api/app/door/open", FarmDoorApp::handleDoorOpen);
  Esp32BaseWeb::addApi("/api/app/door/close", FarmDoorApp::handleDoorClose);
  Esp32BaseWeb::addApi("/api/app/door/stop", FarmDoorApp::handleDoorStop);
  Esp32BaseWeb::addApi("/api/app/maintenance/set-position", FarmDoorApp::handleSetPosition);
  Esp32BaseWeb::addApi("/api/app/maintenance/set-travel", FarmDoorApp::handleSetTravel);
  Esp32BaseWeb::addApi("/api/app/maintenance/adjust-travel", FarmDoorApp::handleAdjustTravel);
  Esp32BaseWeb::addApi("/api/app/maintenance/clear-fault", FarmDoorApp::handleClearFault);
#endif
}

void FarmDoorApp::sendHomePage() {
#if ESP32BASE_ENABLE_WEB
  const DoorSnapshot snapshot = g_door.snapshot();
  char positionValue[32];
  char targetValue[32];
  char percentValue[16];
  char maxRunValue[32];
  formatInt64WithUnit(positionValue, sizeof(positionValue), snapshot.positionPulses, "脉冲");
  formatInt64WithUnit(targetValue, sizeof(targetValue), snapshot.openTargetPulses, "脉冲");
  formatPercent(percentValue, sizeof(percentValue), positionPercent(snapshot));
  formatInt64WithUnit(maxRunValue, sizeof(maxRunValue), g_doorConfig.maxRunPulses, "脉冲");

  Esp32BaseWeb::sendHeader("自动门首页");
  Esp32BaseWeb::sendPageTitle("自动门首页", "鸡舍自动门状态和操作");

  Esp32BaseWeb::beginPanel("门状态");
  sendDoorStatusTable(snapshot, positionValue, targetValue, percentValue, maxRunValue);
  if (!g_motorOutputEnabled) {
    Esp32BaseWeb::sendNotice(Esp32BaseWeb::UI_WARN,
                             "电机输出未启用",
                             "开门和关门命令会被保护拒绝；请在 Esp32Base App Config 中启用。");
  }
  sendDoorActionButtons(snapshot);
  Esp32BaseWeb::endPanel();
  Esp32BaseWeb::sendFooter();
#endif
}

void FarmDoorApp::sendRecordsPage() {
#if ESP32BASE_ENABLE_WEB
  PageParams params;
  bool valid = readPageParams(15, 15, params);
  uint32_t startIndex = 0;
  valid = valid && pageStartIndex(params, startIndex);

  DoorRecordQuery query;
  query.startIndex = startIndex;
  query.limit = static_cast<uint8_t>(params.per);
  char eventType[32] = {};
  if (valid) {
    valid = readUint32ParamOptional("startUnixTime", query.startUnixTime) &&
            readUint32ParamOptional("endUnixTime", query.endUnixTime);
  }
  if (Esp32BaseWeb::getParam("eventType", eventType, sizeof(eventType)) &&
      eventType[0] != '\0') {
    valid = valid && doorRecordTypeFromName(eventType, query.type);
    query.typeFilterEnabled = valid;
  }
  uint32_t archiveIndex = 0;
  if (valid) {
    valid = readUint32ParamOptional("archive", archiveIndex) &&
            archiveIndex <= kDoorRecordMaxArchives;
  }

  char recordQuery[192];
  if (query.typeFilterEnabled) {
    snprintf(recordQuery,
             sizeof(recordQuery),
             "archive=%lu&startUnixTime=%lu&endUnixTime=%lu&eventType=%s",
             static_cast<unsigned long>(archiveIndex),
             static_cast<unsigned long>(query.startUnixTime),
             static_cast<unsigned long>(query.endUnixTime),
             eventType);
  } else {
    snprintf(recordQuery,
             sizeof(recordQuery),
             "archive=%lu&startUnixTime=%lu&endUnixTime=%lu",
             static_cast<unsigned long>(archiveIndex),
             static_cast<unsigned long>(query.startUnixTime),
             static_cast<unsigned long>(query.endUnixTime));
  }

  Esp32BaseWeb::sendHeader("开关门记录");
  Esp32BaseWeb::sendPageTitle("开关门记录", "分页查看自动门操作、校准和维护记录");

  Esp32BaseWeb::beginPanel("筛选");
  Esp32BaseWeb::sendChunk("<form class='editform' method='get' action='/records'>");
  Esp32BaseWeb::sendChunk("<div class='fieldgrid'>");
  char value[16];
  snprintf(value, sizeof(value), "%lu", static_cast<unsigned long>(params.per));
  Esp32BaseWeb::sendChunk("<div class='field short'><label for='rec-per'>每页</label><input id='rec-per' name='per' type='number' min='1' max='15' value='");
  Esp32BaseWeb::sendChunk(value);
  Esp32BaseWeb::sendChunk("'></div>");
  snprintf(value, sizeof(value), "%lu", static_cast<unsigned long>(archiveIndex));
  Esp32BaseWeb::sendChunk("<div class='field short'><label for='rec-archive'>归档</label><input id='rec-archive' name='archive' type='number' min='0' max='16' value='");
  Esp32BaseWeb::sendChunk(value);
  Esp32BaseWeb::sendChunk("'><small>0 为当前文件，1-16 为轮转归档。</small></div>");
  snprintf(value, sizeof(value), "%lu", static_cast<unsigned long>(query.startUnixTime));
  Esp32BaseWeb::sendChunk("<div class='field med'><label for='rec-start-time'>开始时间戳</label><input id='rec-start-time' name='startUnixTime' type='number' min='0' value='");
  Esp32BaseWeb::sendChunk(value);
  Esp32BaseWeb::sendChunk("'></div>");
  snprintf(value, sizeof(value), "%lu", static_cast<unsigned long>(query.endUnixTime));
  Esp32BaseWeb::sendChunk("<div class='field med'><label for='rec-end-time'>结束时间戳</label><input id='rec-end-time' name='endUnixTime' type='number' min='0' value='");
  Esp32BaseWeb::sendChunk(value);
  Esp32BaseWeb::sendChunk("'></div>");
  Esp32BaseWeb::sendChunk("<div class='field med'><label for='rec-type'>事件类型</label><input id='rec-type' name='eventType' placeholder='DoorTravelSet' value='");
  Esp32BaseWeb::writeHtmlEscaped(eventType);
  Esp32BaseWeb::sendChunk("'></div>");
  Esp32BaseWeb::sendChunk("</div><div class='actions'><input type='submit' value='筛选'></div></form>");
  Esp32BaseWeb::endPanel();

  if (!valid) {
    Esp32BaseWeb::sendNotice(Esp32BaseWeb::UI_WARN,
                             "参数无效",
                             "请检查页码、每页数量、归档编号、时间戳或事件类型。");
  }

  Esp32BaseWeb::beginPanel("记录列表");
  Esp32BaseWeb::sendChunk("<div class='tablewrap'><table class='part'>");
  Esp32BaseWeb::sendChunk("<tr><th>序号</th><th>时间</th><th>事件类型</th><th>命令</th><th>结果</th><th>位置变化</th><th>行程变化</th></tr>");

  uint32_t totalRecords = 0;
  uint8_t emitted = 0;
  if (valid) {
#if ESP32BASE_ENABLE_FS
    char recordPath[96];
    DoorRecordPage page;
    const bool hasPath = doorRecordPathForArchive(archiveIndex, recordPath, sizeof(recordPath));
    const DoorRecordReadResult readResult =
        hasPath ? readDoorRecordPage(recordPath,
                                     query,
                                     doorRecordFileSize,
                                     readDoorRecordBytesAt,
                                     nullptr,
                                     page)
                : DoorRecordReadResult::InvalidArgument;
    if (readResult == DoorRecordReadResult::Ok) {
      totalRecords = page.totalRecords;
      for (uint8_t i = 0; i < page.count; ++i) {
        sendRecordHtmlRow(page.records[i]);
        ++emitted;
      }
    } else if (archiveIndex > 0) {
      Esp32BaseWeb::sendNotice(Esp32BaseWeb::UI_WARN,
                               "归档读取失败",
                               "指定归档暂时不可读或记录文件损坏。");
    }
    if (readResult != DoorRecordReadResult::Ok && archiveIndex == 0)
#endif
    {
      const DoorRecordSnapshot snapshot = g_records.snapshot();
      totalRecords = snapshot.count;
      for (uint8_t i = 0; i < snapshot.count; ++i) {
        if (i < startIndex || emitted >= params.per) {
          continue;
        }
        const DoorRecord& record = snapshot.records[i];
        if (query.startUnixTime > 0 && record.unixTime < query.startUnixTime) {
          continue;
        }
        if (query.endUnixTime > 0 && record.unixTime > query.endUnixTime) {
          continue;
        }
        if (query.typeFilterEnabled && record.type != query.type) {
          continue;
        }
        sendRecordHtmlRow(record);
        ++emitted;
      }
    }
  }
  if (emitted == 0) {
    Esp32BaseWeb::sendChunk("<tr><td colspan='7'>暂无记录</td></tr>");
  }
  Esp32BaseWeb::sendChunk("</table></div>");
  Esp32BaseWeb::Pagination recordPagination = {"/records", recordQuery, params.page, params.per, totalRecords};
  Esp32BaseWeb::sendPagination(recordPagination);
  Esp32BaseWeb::endPanel();
  Esp32BaseWeb::sendFooter();
#endif
}

void FarmDoorApp::sendEventsPage() {
#if ESP32BASE_ENABLE_WEB
  PageParams params;
  bool valid = readPageParams(20, 50, params);
  uint32_t startIndex = 0;
  valid = valid && pageStartIndex(params, startIndex) && startIndex <= UINT16_MAX;

  Esp32BaseWeb::sendHeader("业务事件");
  Esp32BaseWeb::sendPageTitle("业务事件", "分页查看维护、保护和存储告警事件");

  if (!FarmAutoEventLog::isReady()) {
    Esp32BaseWeb::sendNotice(Esp32BaseWeb::UI_WARN,
                             "业务事件存储未就绪",
                             FarmAutoEventLog::lastError());
  }
  if (!valid) {
    Esp32BaseWeb::sendNotice(Esp32BaseWeb::UI_WARN,
                             "参数无效",
                             "请检查页码或每页数量。");
  }

  Esp32BaseWeb::beginPanel("事件列表");
  Esp32BaseWeb::sendChunk("<div class='tablewrap'><table class='part'>");
  Esp32BaseWeb::sendChunk("<tr><th>ID</th><th>时间</th><th>级别</th><th>领域</th><th>动作</th><th>目标</th><th>消息</th><th>详情</th></tr>");

  BusinessEventTableState state;
  bool readOk = false;
  if (valid) {
    readOk = FarmAutoEventLog::readLatest(static_cast<uint16_t>(startIndex),
                                          static_cast<uint16_t>(params.per),
                                          sendBusinessEventHtmlCallback,
                                          &state);
  }
  if (state.emitted == 0) {
    Esp32BaseWeb::sendChunk("<tr><td colspan='8'>暂无事件</td></tr>");
  }
  Esp32BaseWeb::sendChunk("</table></div>");
  if (valid && !readOk) {
    Esp32BaseWeb::sendNotice(Esp32BaseWeb::UI_WARN,
                             "业务事件读取失败",
                             FarmAutoEventLog::lastError());
  }
  Esp32BaseWeb::Pagination eventPagination = {"/events", nullptr, params.page, params.per, FarmAutoEventLog::count()};
  Esp32BaseWeb::sendPagination(eventPagination);
  Esp32BaseWeb::endPanel();
  Esp32BaseWeb::sendFooter();
#endif
}

void FarmDoorApp::sendCalibrationPage() {
#if ESP32BASE_ENABLE_WEB
  const DoorSnapshot snapshot = g_door.snapshot();
  char positionValue[32];
  char targetValue[32];
  char pulsesPerRevValue[32];
  char turnsValue[32];
  const int64_t turnsX100 = turnsX100FromOpenTargetPulses(snapshot.openTargetPulses);
  formatInt64WithUnit(positionValue, sizeof(positionValue), snapshot.positionPulses, "脉冲");
  formatInt64WithUnit(targetValue, sizeof(targetValue), snapshot.openTargetPulses, "脉冲");
  formatInt64(pulsesPerRevValue, sizeof(pulsesPerRevValue), g_motorKinematics.outputPulsesPerRev);
  snprintf(turnsValue,
           sizeof(turnsValue),
           "%lld.%02lld 圈",
           static_cast<long long>(turnsX100 / 100),
           static_cast<long long>(llabs(turnsX100 % 100)));

  Esp32BaseWeb::sendHeader("行程校准");
  Esp32BaseWeb::sendPageTitle("校准", "标定当前位置、开门目标和行程微调");

  Esp32BaseWeb::beginPanel("当前位置");
  sendCalibrationStatusTable(snapshot, positionValue, targetValue, turnsValue, pulsesPerRevValue);
  Esp32BaseWeb::endPanel();

  Esp32BaseWeb::beginPanel("端点标定");
  Esp32BaseWeb::sendChunk("<form class='editform' method='post' action='/api/app/maintenance/set-position' onsubmit=\"return confirm('把当前位置标为关门基准？')&&once(this)\">");
  Esp32BaseWeb::sendChunk("<input type='hidden' name='position' value='closed'>");
  Esp32BaseWeb::sendChunk("<div class='actions'><input class='danger' type='submit' value='把当前位置标为关门基准'></div></form>");
  Esp32BaseWeb::sendChunk("<form class='editform' method='post' action='/api/app/maintenance/set-position' onsubmit=\"return confirm('把当前位置标为开门位置？')&&once(this)\">");
  Esp32BaseWeb::sendChunk("<input type='hidden' name='position' value='open'>");
  Esp32BaseWeb::sendChunk("<div class='actions'><input class='danger' type='submit' value='把当前位置标为开门位置'></div></form>");
  Esp32BaseWeb::endPanel();

  Esp32BaseWeb::beginPanel("行程调整");
  Esp32BaseWeb::sendChunk("<form class='editform' method='post' action='/api/app/maintenance/set-travel' onsubmit=\"return confirm('设置新的开门目标？')&&once(this)\">");
  Esp32BaseWeb::sendChunk("<div class='fieldgrid'>");
  Esp32BaseWeb::sendChunk("<div class='field med'><label for='travel-turns'>开门目标</label><input id='travel-turns' name='openTurnsX100' type='number' min='1' max='2000' value='500'><small>单位为 0.01 圈。</small></div>");
  Esp32BaseWeb::sendChunk("</div><div class='actions'><input class='warn' type='submit' value='设置行程'></div></form>");
  Esp32BaseWeb::sendChunk("<form class='editform' method='post' action='/api/app/maintenance/adjust-travel' onsubmit=\"return confirm('微调开门目标？')&&once(this)\">");
  Esp32BaseWeb::sendChunk("<div class='fieldgrid'>");
  Esp32BaseWeb::sendChunk("<div class='field med'><label for='adjust-delta'>微调量</label><input id='adjust-delta' name='deltaTurnsX100' type='number' value='5'><small>单位为 0.01 圈，可为负数。</small></div>");
  Esp32BaseWeb::sendChunk("</div><div class='actions'><input class='warn' type='submit' value='微调行程'></div></form>");
  Esp32BaseWeb::endPanel();
  Esp32BaseWeb::sendFooter();
#endif
}

void FarmDoorApp::sendDiagnosticsPage() {
#if ESP32BASE_ENABLE_WEB
  char at24cValue[16];
  char currentValue[32];
  const FarmDoorReadOnlyDiagnostics diagnostics = FarmDoorHw.readDiagnostics();
  snprintf(at24cValue, sizeof(at24cValue), "%s", diagnostics.at24cOnline ? "在线" : "离线");
  snprintf(currentValue, sizeof(currentValue), "%d ADC", diagnostics.currentRawAdc);

  Esp32BaseWeb::sendHeader("自动门诊断");
  Esp32BaseWeb::sendPageTitle("诊断", "只读硬件状态");

  Esp32BaseWeb::beginPanel("硬件状态");
  sendDiagnosticsStatusTable(diagnostics, at24cValue, currentValue);
  Esp32BaseWeb::endPanel();
  Esp32BaseWeb::sendFooter();
#endif
}

void FarmDoorApp::sendStatusJson() {
#if ESP32BASE_ENABLE_WEB
  if (!requireApiAuth()) {
    return;
  }
  const DoorSnapshot snapshot = g_door.snapshot();
  beginRawJson(200);
  Esp32BaseWeb::sendChunk("{\"appKind\":\"FarmDoor\",");
  Esp32BaseWeb::sendChunk("\"firmware\":\"");
  Esp32BaseWeb::writeJsonEscaped(Esp32Base::firmwareVersion());
  Esp32BaseWeb::sendChunk("\",\"schemaVersion\":1,");
  Esp32BaseWeb::sendChunk("\"state\":\"");
  Esp32BaseWeb::sendChunk(stateName(snapshot.state));
  Esp32BaseWeb::sendChunk("\",\"activeCommand\":\"");
  Esp32BaseWeb::sendChunk(commandName(snapshot.activeCommand));
  Esp32BaseWeb::sendChunk("\",\"position\":{\"pulses\":");
  sendInt64(snapshot.positionPulses);
  Esp32BaseWeb::sendChunk(",\"percent\":");
  sendUint32(positionPercent(snapshot));
  Esp32BaseWeb::sendChunk(",\"trustLevel\":\"");
  Esp32BaseWeb::sendChunk(trustName(snapshot.positionTrustLevel));
  Esp32BaseWeb::sendChunk("\",\"source\":\"");
  Esp32BaseWeb::sendChunk(snapshot.positionTrustLevel == PositionTrustLevel::Untrusted ? "NotCalibrated" : "Runtime");
  Esp32BaseWeb::sendChunk("\"},\"travel\":{\"closedPulses\":");
  sendInt64(snapshot.closedPositionPulses);
  Esp32BaseWeb::sendChunk(",\"openTargetPulses\":");
  sendInt64(snapshot.openTargetPulses);
  Esp32BaseWeb::sendChunk(",\"outputPulsesPerRev\":");
  sendInt64(kDefaultOutputPulsesPerRev);
  Esp32BaseWeb::sendChunk(",\"maxRunPulses\":");
  sendInt64(g_doorConfig.maxRunPulses);
  Esp32BaseWeb::sendChunk(",\"maxRunMs\":");
  sendUint32(g_doorConfig.maxRunMs);
  Esp32BaseWeb::sendChunk("},\"lastStopReason\":\"");
  Esp32BaseWeb::sendChunk(stopReasonName(snapshot.lastStopReason));
  Esp32BaseWeb::sendChunk("\",\"faultReason\":\"");
  Esp32BaseWeb::sendChunk(faultReasonName(snapshot.faultReason));
  Esp32BaseWeb::sendChunk("\",\"recentCommand\":");
  sendRecentCommandJson();
  Esp32BaseWeb::sendChunk(",");
  Esp32BaseWeb::sendChunk("\"currentSensor\":{\"chip\":\"INA240A2\",\"adcPin\":33,\"enabled\":");
  Esp32BaseWeb::sendChunk(g_currentGuard.enabled ? "true" : "false");
  Esp32BaseWeb::sendChunk("},\"motor\":{\"driver\":\"AT8236\",\"encoderMode\":\"X1\"},");
  sendMotorOutputJson();
  Esp32BaseWeb::sendChunk("}");
  endRawJson();
#endif
}

void FarmDoorApp::sendDiagnosticsJson() {
#if ESP32BASE_ENABLE_WEB
  if (!requireApiAuth()) {
    return;
  }
  const FarmDoorReadOnlyDiagnostics diagnostics = FarmDoorHw.readDiagnostics();

  beginRawJson(200);
  Esp32BaseWeb::sendChunk("{\"appKind\":\"FarmDoor\",");
  Esp32BaseWeb::sendChunk("\"mode\":\"readOnlyDiagnostics\",");
  Esp32BaseWeb::sendChunk("\"buttons\":{");
  sendDigitalField("aux", diagnostics.buttonAux);
  Esp32BaseWeb::sendChunk(",");
  sendDigitalField("open", diagnostics.buttonOpen);
  Esp32BaseWeb::sendChunk(",");
  sendDigitalField("close", diagnostics.buttonClose);
  Esp32BaseWeb::sendChunk(",");
  sendDigitalField("stop", diagnostics.buttonStop);
  Esp32BaseWeb::sendChunk("},\"encoder\":{");
  sendDigitalField("a", diagnostics.encoderA);
  Esp32BaseWeb::sendChunk(",");
  sendDigitalField("b", diagnostics.encoderB);
  Esp32BaseWeb::sendChunk("},\"currentSensor\":{\"chip\":\"INA240A2\",\"adcPin\":33,\"rawAdc\":");
  char number[16];
  snprintf(number, sizeof(number), "%d", diagnostics.currentRawAdc);
  Esp32BaseWeb::sendChunk(number);
  Esp32BaseWeb::sendChunk(",\"rawMin\":");
  snprintf(number, sizeof(number), "%d", diagnostics.currentRawMin);
  Esp32BaseWeb::sendChunk(number);
  Esp32BaseWeb::sendChunk(",\"rawMax\":");
  snprintf(number, sizeof(number), "%d", diagnostics.currentRawMax);
  Esp32BaseWeb::sendChunk(number);
  Esp32BaseWeb::sendChunk(",\"rawAvg\":");
  snprintf(number, sizeof(number), "%d", diagnostics.currentRawAvg);
  Esp32BaseWeb::sendChunk(number);
  Esp32BaseWeb::sendChunk(",\"sampleCount\":");
  snprintf(number, sizeof(number), "%u", diagnostics.currentSampleCount);
  Esp32BaseWeb::sendChunk(number);
  Esp32BaseWeb::sendChunk(",\"compileEnabled\":");
#if FARMAUTO_FARMDOOR_ENABLE_INA240A2
  Esp32BaseWeb::sendChunk("true");
#else
  Esp32BaseWeb::sendChunk("false");
#endif
  Esp32BaseWeb::sendChunk(",\"runtimeEnabled\":");
  Esp32BaseWeb::sendChunk(boolJson(g_currentGuard.enabled));
  Esp32BaseWeb::sendChunk("},\"at24c\":{\"address\":\"0x50\",\"online\":");
  Esp32BaseWeb::sendChunk(boolJson(diagnostics.at24cOnline));
  Esp32BaseWeb::sendChunk(",\"storeReady\":");
  Esp32BaseWeb::sendChunk(boolJson(g_at24cStoreReady));
  Esp32BaseWeb::sendChunk("},");
  sendMotorOutputJson();
  Esp32BaseWeb::sendChunk("}");
  endRawJson();
#endif
}

void FarmDoorApp::sendRecentEventsJson() {
#if ESP32BASE_ENABLE_WEB
  if (!requireApiAuth()) {
    return;
  }
  uint32_t limitParam = 32;
  uint32_t offsetParam = 0;
  if (!readUint32ParamOptional("limit", limitParam) ||
      !readUint32ParamOptional("start", offsetParam) ||
      limitParam == 0 || limitParam > 100 || offsetParam > UINT16_MAX) {
    beginRawJson(400);
    Esp32BaseWeb::sendChunk("{\"ok\":false,\"error\":\"InvalidArgument\"}");
    endRawJson();
    return;
  }
  BusinessEventJsonState state;
  beginRawJson(200);
  Esp32BaseWeb::sendChunk("{\"store\":\"app_events\",\"ready\":");
  Esp32BaseWeb::sendChunk(boolJson(FarmAutoEventLog::isReady()));
  Esp32BaseWeb::sendChunk(",\"faulted\":");
  Esp32BaseWeb::sendChunk(boolJson(FarmAutoEventLog::faulted()));
  Esp32BaseWeb::sendChunk(",\"count\":");
  sendUint32(FarmAutoEventLog::count());
  Esp32BaseWeb::sendChunk(",\"capacity\":");
  sendUint32(FarmAutoEventLog::capacity());
  Esp32BaseWeb::sendChunk(",\"events\":[");
  const bool readOk = FarmAutoEventLog::readLatest(static_cast<uint16_t>(offsetParam),
                                                   static_cast<uint16_t>(limitParam),
                                                   sendBusinessEventJsonCallback,
                                                   &state);
  Esp32BaseWeb::sendChunk("],\"readOk\":");
  Esp32BaseWeb::sendChunk(boolJson(readOk));
  Esp32BaseWeb::sendChunk(",\"returned\":");
  sendUint32(state.emitted);
  Esp32BaseWeb::sendChunk(",\"lastError\":\"");
  Esp32BaseWeb::writeJsonEscaped(FarmAutoEventLog::lastError());
  Esp32BaseWeb::sendChunk("\"}");
  endRawJson();
#endif
}

void FarmDoorApp::sendRecordsJson() {
#if ESP32BASE_ENABLE_WEB
  if (!requireApiAuth()) {
    return;
  }
  DoorRecordQuery query;
  uint32_t limitParam = query.limit;
  if (!readUint32ParamOptional("start", query.startIndex) ||
      !readUint32ParamOptional("limit", limitParam) ||
      !readUint32ParamOptional("startUnixTime", query.startUnixTime) ||
      !readUint32ParamOptional("endUnixTime", query.endUnixTime) || limitParam == 0 ||
      limitParam > kDoorRecordPageMaxRecords) {
    sendCommandResultJson(DoorCommandResult::InvalidArgument);
    return;
  }
  query.limit = static_cast<uint8_t>(limitParam);
  char eventType[32];
  if (Esp32BaseWeb::getParam("eventType", eventType, sizeof(eventType)) && eventType[0] != '\0') {
    if (!doorRecordTypeFromName(eventType, query.type)) {
      sendCommandResultJson(DoorCommandResult::InvalidArgument);
      return;
    }
    query.typeFilterEnabled = true;
  }
  uint32_t archiveIndex = 0;
  if (!readUint32ParamOptional("archive", archiveIndex) || archiveIndex > kDoorRecordMaxArchives) {
    sendCommandResultJson(DoorCommandResult::InvalidArgument);
    return;
  }

  beginRawJson(200);
#if ESP32BASE_ENABLE_FS
  char recordPath[96];
  if (!doorRecordPathForArchive(archiveIndex, recordPath, sizeof(recordPath))) {
    sendCommandResultJson(DoorCommandResult::InvalidArgument);
    return;
  }
  DoorRecordPage page;
  const DoorRecordReadResult readResult = readDoorRecordPage(recordPath,
                                                             query,
                                                             doorRecordFileSize,
                                                             readDoorRecordBytesAt,
                                                             nullptr,
                                                             page);
  if (readResult == DoorRecordReadResult::Ok && page.totalRecords > 0) {
    Esp32BaseWeb::sendChunk("{\"source\":\"flash\",\"start\":");
    sendUint32(page.startIndex);
    Esp32BaseWeb::sendChunk(",\"archive\":");
    sendUint32(archiveIndex);
    Esp32BaseWeb::sendChunk(",\"nextIndex\":");
    sendUint32(page.nextIndex);
    Esp32BaseWeb::sendChunk(",\"limit\":");
    sendUint32(limitParam);
    Esp32BaseWeb::sendChunk(",\"count\":");
    sendUint32(page.count);
    Esp32BaseWeb::sendChunk(",\"totalRecords\":");
    sendUint32(page.totalRecords);
    Esp32BaseWeb::sendChunk(",\"recordBytes\":");
    sendUint32(kDoorRecordEncodedMaxBytes);
    Esp32BaseWeb::sendChunk(",\"records\":[");
    for (uint8_t i = 0; i < page.count; ++i) {
      if (i > 0) {
        Esp32BaseWeb::sendChunk(",");
      }
      sendRecordJson(page.records[i]);
    }
    Esp32BaseWeb::sendChunk("]}");
    endRawJson();
    return;
  }
  if (archiveIndex > 0) {
    Esp32BaseWeb::sendChunk("{\"source\":\"flash\",\"start\":0,\"archive\":");
    sendUint32(archiveIndex);
    Esp32BaseWeb::sendChunk(",\"nextIndex\":0,\"limit\":");
    sendUint32(limitParam);
    Esp32BaseWeb::sendChunk(",\"count\":0,\"totalRecords\":0,\"recordBytes\":");
    sendUint32(kDoorRecordEncodedMaxBytes);
    Esp32BaseWeb::sendChunk(",\"records\":[]}");
    endRawJson();
    return;
  }
#endif

  const DoorRecordSnapshot snapshot = g_records.snapshot();
  Esp32BaseWeb::sendChunk("{\"source\":\"ram\",\"start\":0,\"limit\":");
  sendUint32(kDoorRecentRecordCapacity);
  Esp32BaseWeb::sendChunk(",\"count\":");
  sendUint32(snapshot.count);
  Esp32BaseWeb::sendChunk(",\"totalRecords\":");
  sendUint32(snapshot.count);
  Esp32BaseWeb::sendChunk(",\"capacity\":");
  sendUint32(kDoorRecentRecordCapacity);
  Esp32BaseWeb::sendChunk(",\"records\":[");
  for (uint8_t i = 0; i < snapshot.count; ++i) {
    if (i > 0) {
      Esp32BaseWeb::sendChunk(",");
    }
    sendRecordJson(snapshot.records[i]);
  }
  Esp32BaseWeb::sendChunk("]}");
  endRawJson();
#endif
}

void FarmDoorApp::handleDoorOpen() {
#if ESP32BASE_ENABLE_WEB
  if (!requireApiAuth()) {
    return;
  }
  const uint32_t commandId = allocateCommandId();
  DoorCommandResult result = DoorCommandResult::InvalidArgument;
  if (g_motorOutputEnabled) {
    result = g_door.requestOpen();
    if (result == DoorCommandResult::Ok) {
      result = startDoorMotorTo(g_door.snapshot().targetPulses);
    }
  }
  DoorRecord record;
  record.commandId = commandId;
  record.type = DoorRecordType::CommandRequested;
  record.result = doorRecordResultFromCommand(result);
  record.command = DoorCommand::Open;
  record.newTravelPulses = g_door.snapshot().openTargetPulses;
  recordBusinessEvent(record);
  sendCommandResultJson(result, commandId);
#endif
}

void FarmDoorApp::handleDoorClose() {
#if ESP32BASE_ENABLE_WEB
  if (!requireApiAuth()) {
    return;
  }
  const uint32_t commandId = allocateCommandId();
  DoorCommandResult result = DoorCommandResult::InvalidArgument;
  if (g_motorOutputEnabled) {
    result = g_door.requestClose();
    if (result == DoorCommandResult::Ok) {
      result = startDoorMotorTo(g_door.snapshot().targetPulses);
    }
  }
  DoorRecord record;
  record.commandId = commandId;
  record.type = DoorRecordType::CommandRequested;
  record.result = doorRecordResultFromCommand(result);
  record.command = DoorCommand::Close;
  recordBusinessEvent(record);
  sendCommandResultJson(result, commandId);
#endif
}

void FarmDoorApp::handleDoorStop() {
#if ESP32BASE_ENABLE_WEB
  if (!requireApiAuth()) {
    return;
  }
  const uint32_t commandId = allocateCommandId();
  const DoorSnapshot snapshot = g_door.snapshot();
  int64_t stoppedPosition = snapshot.positionPulses;
  if (g_motorOutputReady) {
    g_motor.requestStop();
    stoppedPosition = g_motor.snapshot().positionPulses;
  }
  const DoorCommandResult result = g_door.requestStop(stoppedPosition);
  DoorRecord record;
  record.commandId = commandId;
  record.type = DoorRecordType::CommandRequested;
  record.result = doorRecordResultFromCommand(result);
  record.command = DoorCommand::Stop;
  record.oldPositionPulses = snapshot.positionPulses;
  record.newPositionPulses = stoppedPosition;
  recordBusinessEvent(record);
  if (result == DoorCommandResult::Ok) {
    persistDoorRecoveryStateIfReady();
  }
  sendCommandResultJson(result, commandId);
#endif
}

void FarmDoorApp::handleSetPosition() {
#if ESP32BASE_ENABLE_WEB
  if (!requireApiAuth()) {
    return;
  }
  const DoorSnapshot before = g_door.snapshot();
  DoorCommandResult result = DoorCommandResult::InvalidArgument;
  char position[16];
  if (Esp32BaseWeb::getParam("position", position, sizeof(position))) {
    if (strcmp(position, "closed") == 0 || strcmp(position, "Closed") == 0) {
      const uint32_t commandId = allocateCommandId();
      result = g_door.markPositionClosed();
      DoorRecord record;
      record.commandId = commandId;
      record.type = DoorRecordType::PositionSet;
      record.result = doorRecordResultFromCommand(result);
      record.oldPositionPulses = before.positionPulses;
      record.newPositionPulses = g_door.snapshot().positionPulses;
      recordBusinessEvent(record);
      if (result == DoorCommandResult::Ok) {
        if (g_motorOutputReady) {
          g_motorEncoder.resetPosition(g_door.snapshot().positionPulses);
          g_motor.setCurrentPositionPulses(g_door.snapshot().positionPulses);
        }
        persistDoorRecoveryStateIfReady();
      }
      sendCommandResultJson(result, commandId);
      return;
    }
    if (strcmp(position, "open") == 0 || strcmp(position, "Open") == 0) {
      const uint32_t commandId = allocateCommandId();
      result = g_door.markPositionOpen();
      DoorRecord record;
      record.commandId = commandId;
      record.type = DoorRecordType::PositionSet;
      record.result = doorRecordResultFromCommand(result);
      record.oldPositionPulses = before.positionPulses;
      record.newPositionPulses = g_door.snapshot().positionPulses;
      recordBusinessEvent(record);
      if (result == DoorCommandResult::Ok) {
        if (g_motorOutputReady) {
          g_motorEncoder.resetPosition(g_door.snapshot().positionPulses);
          g_motor.setCurrentPositionPulses(g_door.snapshot().positionPulses);
        }
        persistDoorRecoveryStateIfReady();
      }
      sendCommandResultJson(result, commandId);
      return;
    }
    if (strcmp(position, "unknown") == 0 || strcmp(position, "Unknown") == 0) {
      const uint32_t commandId = allocateCommandId();
      result = g_door.setTrustedPosition(0, PositionTrustLevel::Untrusted);
      DoorRecord record;
      record.commandId = commandId;
      record.type = DoorRecordType::PositionSet;
      record.result = doorRecordResultFromCommand(result);
      record.oldPositionPulses = before.positionPulses;
      record.newPositionPulses = g_door.snapshot().positionPulses;
      recordBusinessEvent(record);
      if (result == DoorCommandResult::Ok) {
        if (g_motorOutputReady) {
          g_motorEncoder.resetPosition(g_door.snapshot().positionPulses);
          g_motor.setCurrentPositionPulses(g_door.snapshot().positionPulses);
        }
        persistDoorRecoveryStateIfReady();
      }
      sendCommandResultJson(result, commandId);
      return;
    }
    sendCommandResultJson(DoorCommandResult::InvalidArgument);
    return;
  }

  int64_t positionPulses = 0;
  PositionTrustLevel trustLevel = PositionTrustLevel::Trusted;
  if (!readInt64Param("positionPulses", positionPulses) || !readTrustLevelParam(trustLevel)) {
    sendCommandResultJson(DoorCommandResult::InvalidArgument);
    return;
  }
  const uint32_t commandId = allocateCommandId();
  result = g_door.setTrustedPosition(positionPulses, trustLevel);
  DoorRecord record;
  record.commandId = commandId;
  record.type = DoorRecordType::PositionSet;
  record.result = doorRecordResultFromCommand(result);
  record.oldPositionPulses = before.positionPulses;
  record.newPositionPulses = g_door.snapshot().positionPulses;
  recordBusinessEvent(record);
  if (result == DoorCommandResult::Ok) {
    if (g_motorOutputReady) {
      g_motorEncoder.resetPosition(g_door.snapshot().positionPulses);
      g_motor.setCurrentPositionPulses(g_door.snapshot().positionPulses);
    }
    persistDoorRecoveryStateIfReady();
  }
  sendCommandResultJson(result, commandId);
#endif
}

void FarmDoorApp::handleSetTravel() {
#if ESP32BASE_ENABLE_WEB
  if (!requireApiAuth()) {
    return;
  }
  int64_t openTargetPulses = 0;
  int64_t openTurnsX100 = 0;
  const bool hasPulses = hasParam("openTargetPulses");
  const bool hasTurns = hasParam("openTurnsX100");
  if (hasPulses) {
    if (!readInt64Param("openTargetPulses", openTargetPulses)) {
      sendTravelResultJson(DoorCommandResult::InvalidArgument, false);
      return;
    }
  } else if (hasTurns) {
    if (!readInt64Param("openTurnsX100", openTurnsX100)) {
      sendTravelResultJson(DoorCommandResult::InvalidArgument, false);
      return;
    }
    openTargetPulses = openTargetPulsesFromTurnsX100(openTurnsX100);
  } else {
    sendTravelResultJson(DoorCommandResult::InvalidArgument, false);
    return;
  }

  int64_t maxRunPulses = defaultMaxRunPulsesForTarget(openTargetPulses);
  if (!readInt64ParamOptional("maxRunPulses", maxRunPulses)) {
    sendTravelResultJson(DoorCommandResult::InvalidArgument, false);
    return;
  }
  const uint32_t commandId = allocateCommandId();
  bool configSaved = false;
  const int64_t oldTravelPulses = g_door.snapshot().openTargetPulses;
  const DoorCommandResult result = applyTravelUpdate(openTargetPulses, maxRunPulses, configSaved);
  DoorRecord record;
  record.commandId = commandId;
  record.type = DoorRecordType::TravelSet;
  record.result = doorRecordResultFromCommand(result);
  record.oldTravelPulses = oldTravelPulses;
  record.newTravelPulses = g_door.snapshot().openTargetPulses;
  record.deltaPulses = record.newTravelPulses - record.oldTravelPulses;
  recordBusinessEvent(record);
  if (result == DoorCommandResult::Ok) {
    persistDoorRecoveryStateIfReady();
  }
  sendTravelResultJson(result, configSaved, commandId);
#endif
}

void FarmDoorApp::handleAdjustTravel() {
#if ESP32BASE_ENABLE_WEB
  if (!requireApiAuth()) {
    return;
  }
  int64_t deltaPulses = 0;
  int64_t deltaTurnsX100 = 0;
  const bool hasDeltaPulses = hasParam("deltaPulses");
  const bool hasDeltaTurns = hasParam("deltaTurnsX100");
  if (hasDeltaPulses) {
    if (!readInt64Param("deltaPulses", deltaPulses)) {
      sendTravelResultJson(DoorCommandResult::InvalidArgument, false);
      return;
    }
  } else if (hasDeltaTurns) {
    if (!readInt64Param("deltaTurnsX100", deltaTurnsX100)) {
      sendTravelResultJson(DoorCommandResult::InvalidArgument, false);
      return;
    }
    deltaPulses = openTargetPulsesFromTurnsX100(deltaTurnsX100);
  } else {
    sendTravelResultJson(DoorCommandResult::InvalidArgument, false);
    return;
  }

  const int64_t openTargetPulses = g_door.snapshot().openTargetPulses + deltaPulses;
  const int64_t maxRunPulses = defaultMaxRunPulsesForTarget(openTargetPulses);
  const uint32_t commandId = allocateCommandId();
  bool configSaved = false;
  const int64_t oldTravelPulses = g_door.snapshot().openTargetPulses;
  const DoorCommandResult result = applyTravelUpdate(openTargetPulses, maxRunPulses, configSaved);
  DoorRecord record;
  record.commandId = commandId;
  record.type = DoorRecordType::TravelAdjusted;
  record.result = doorRecordResultFromCommand(result);
  record.oldTravelPulses = oldTravelPulses;
  record.newTravelPulses = g_door.snapshot().openTargetPulses;
  record.deltaPulses = deltaPulses;
  recordBusinessEvent(record);
  if (result == DoorCommandResult::Ok) {
    persistDoorRecoveryStateIfReady();
  }
  sendTravelResultJson(result, configSaved, commandId);
#endif
}

void FarmDoorApp::handleClearFault() {
#if ESP32BASE_ENABLE_WEB
  if (!requireApiAuth()) {
    return;
  }
  const uint32_t commandId = allocateCommandId();
  const DoorSnapshot before = g_door.snapshot();
  const DoorCommandResult result = g_door.clearFault();
  DoorRecord record;
  record.commandId = commandId;
  record.type = DoorRecordType::FaultCleared;
  record.result = doorRecordResultFromCommand(result);
  recordBusinessEvent(record);
  if (result == DoorCommandResult::Ok) {
    FarmAutoEventLog::recordDoorFaultCleared(faultReasonName(before.faultReason));
  }
  sendCommandResultJson(result, commandId);
#endif
}
