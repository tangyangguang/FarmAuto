#include "FarmDoorApp.h"

#include <Arduino.h>
#include <Esp32At24cRecordStore.h>
#include <Esp32Base.h>
#include <Esp32EncodedDcMotor.h>
#include <Esp32MotorCurrentGuard.h>

#include <cstring>

#include "DoorController.h"
#include "DoorRecordFileStore.h"
#include "DoorRecordLog.h"
#include "FarmDoorHardware.h"

namespace {

constexpr int64_t kDefaultOutputPulsesPerRev = 2096;
constexpr int64_t kDefaultOpenTurnsX100 = 500;
constexpr int64_t kDefaultOpenTargetPulses =
    (kDefaultOutputPulsesPerRev * kDefaultOpenTurnsX100) / 100;
constexpr int64_t kDefaultMaxRunPulses = (kDefaultOpenTargetPulses * 150) / 100;

DoorController g_door;
DoorRecordLog g_records;
bool g_recordStorageReady = false;
DoorControllerConfig g_doorConfig;
Esp32EncodedDcMotor::MotorHardwareConfig g_motorHardware;
Esp32EncodedDcMotor::EncoderBackendConfig g_encoderBackend;
Esp32EncodedDcMotor::MotorKinematics g_motorKinematics;
Esp32EncodedDcMotor::MotorMotionProfile g_motorProfile;
Esp32EncodedDcMotor::MotorProtection g_motorProtection;
Esp32EncodedDcMotor::MotorStopPolicy g_motorStopPolicy;
Esp32MotorCurrentGuard::Ina240A2AnalogConfig g_currentSensor;
Esp32MotorCurrentGuard::MotorCurrentGuardConfig g_currentGuard;
Esp32At24cRecordStore::RecordStoreConfig g_recordStoreConfig;

static constexpr uint8_t kFarmDoorApiRouteCount = 11;
static_assert(ESP32BASE_WEB_MAX_ROUTES >= kFarmDoorApiRouteCount,
              "Esp32FarmDoor requires ESP32BASE_WEB_MAX_ROUTES >= 11");
static constexpr const char* kDoorRecordRootDir = "/records";
static constexpr const char* kDoorRecordDir = "/records/door";
static constexpr const char* kDoorRecordCurrentPath = "/records/door/current.dar";
static constexpr uint32_t kDoorRecordMaxCurrentBytes = 64UL * 1024UL;
static constexpr uint8_t kDoorRecordMaxArchives = 16;

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
  g_currentGuard.enabled = ina240CompileEnabled() && requestedCurrentGuard;
#else
  g_currentGuard.enabled = false;
#endif
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
  }
  const DoorRecordWriteResult result = appendDoorRecordToPath(
      stored, kDoorRecordCurrentPath, appendDoorRecordBytes, nullptr);
  if (result != DoorRecordWriteResult::Ok) {
    ESP32BASE_LOG_W("farmdoor", "record_append_failed result=%u",
                    static_cast<unsigned>(result));
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

void sendRecordSnapshotJson(const char* source) {
  const DoorRecordSnapshot snapshot = g_records.snapshot();
  Esp32BaseWeb::beginJson(200);
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
  Esp32BaseWeb::endJson();
}

void sendCommandResultJson(DoorCommandResult result) {
#if ESP32BASE_ENABLE_WEB
  const DoorSnapshot snapshot = g_door.snapshot();
  Esp32BaseWeb::beginJson(httpCodeFor(result));
  Esp32BaseWeb::sendChunk("{\"result\":\"");
  Esp32BaseWeb::sendChunk(commandResultName(result));
  Esp32BaseWeb::sendChunk("\",\"state\":\"");
  Esp32BaseWeb::sendChunk(stateName(snapshot.state));
  Esp32BaseWeb::sendChunk("\",\"activeCommand\":\"");
  Esp32BaseWeb::sendChunk(commandName(snapshot.activeCommand));
  Esp32BaseWeb::sendChunk("\",\"positionPulses\":");
  sendInt64(snapshot.positionPulses);
  Esp32BaseWeb::sendChunk(",\"targetPulses\":");
  sendInt64(snapshot.targetPulses);
  Esp32BaseWeb::sendChunk(",\"motorOutput\":{\"enabled\":false}}");
  Esp32BaseWeb::endJson();
#endif
}

void sendTravelResultJson(DoorCommandResult result, bool configSaved) {
#if ESP32BASE_ENABLE_WEB
  const DoorSnapshot snapshot = g_door.snapshot();
  Esp32BaseWeb::beginJson(httpCodeFor(result));
  Esp32BaseWeb::sendChunk("{\"result\":\"");
  Esp32BaseWeb::sendChunk(commandResultName(result));
  Esp32BaseWeb::sendChunk("\",\"state\":\"");
  Esp32BaseWeb::sendChunk(stateName(snapshot.state));
  Esp32BaseWeb::sendChunk("\",\"openTargetPulses\":");
  sendInt64(snapshot.openTargetPulses);
  Esp32BaseWeb::sendChunk(",\"openTurnsX100\":");
  sendInt64(turnsX100FromOpenTargetPulses(snapshot.openTargetPulses));
  Esp32BaseWeb::sendChunk(",\"maxRunPulses\":");
  sendInt64(g_doorConfig.maxRunPulses);
  Esp32BaseWeb::sendChunk(",\"configSaved\":");
  Esp32BaseWeb::sendChunk(configSaved ? "true" : "false");
  Esp32BaseWeb::sendChunk(",\"motorOutput\":{\"enabled\":false}}");
  Esp32BaseWeb::endJson();
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
    applyRuntimeConfigFromBase();
    ESP32BASE_LOG_I("farmdoor", "skeleton ready, ina240a2_gpio=%u enabled=%s",
                    FarmDoorHw.pins().currentAdc,
                    g_currentGuard.enabled ? "yes" : "no");
  }
}

void FarmDoorApp::handle() {
  Esp32Base::handle();
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
  Esp32BaseWeb::setHomeMode(Esp32BaseWeb::HOME_ESP32BASE);
  Esp32BaseWeb::setSystemNavMode(Esp32BaseWeb::SYSTEM_NAV_BOTTOM);
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

void FarmDoorApp::sendStatusJson() {
#if ESP32BASE_ENABLE_WEB
  const DoorSnapshot snapshot = g_door.snapshot();
  Esp32BaseWeb::beginJson(200);
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
  Esp32BaseWeb::sendChunk("\",");
  Esp32BaseWeb::sendChunk("\"currentSensor\":{\"chip\":\"INA240A2\",\"adcPin\":33,\"enabled\":");
  Esp32BaseWeb::sendChunk(g_currentGuard.enabled ? "true" : "false");
  Esp32BaseWeb::sendChunk("},\"motor\":{\"driver\":\"AT8236\",\"encoderMode\":\"X1\"}}");
  Esp32BaseWeb::endJson();
#endif
}

void FarmDoorApp::sendDiagnosticsJson() {
#if ESP32BASE_ENABLE_WEB
  const FarmDoorReadOnlyDiagnostics diagnostics = FarmDoorHw.readDiagnostics();

  Esp32BaseWeb::beginJson(200);
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
  Esp32BaseWeb::sendChunk("},\"motorOutput\":{\"enabled\":false}}");
  Esp32BaseWeb::endJson();
#endif
}

void FarmDoorApp::sendRecentEventsJson() {
#if ESP32BASE_ENABLE_WEB
  sendRecordSnapshotJson("ram");
#endif
}

void FarmDoorApp::sendRecordsJson() {
#if ESP32BASE_ENABLE_WEB
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

  Esp32BaseWeb::beginJson(200);
#if ESP32BASE_ENABLE_FS
  DoorRecordPage page;
  const DoorRecordReadResult readResult = readDoorRecordPage(kDoorRecordCurrentPath,
                                                             query,
                                                             doorRecordFileSize,
                                                             readDoorRecordBytesAt,
                                                             nullptr,
                                                             page);
  if (readResult == DoorRecordReadResult::Ok && page.totalRecords > 0) {
    Esp32BaseWeb::sendChunk("{\"source\":\"flash\",\"start\":");
    sendUint32(page.startIndex);
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
    Esp32BaseWeb::endJson();
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
  Esp32BaseWeb::endJson();
#endif
}

void FarmDoorApp::handleDoorOpen() {
#if ESP32BASE_ENABLE_WEB
  const DoorCommandResult result = g_door.requestOpen();
  DoorRecord record;
  record.type = DoorRecordType::CommandRequested;
  record.result = doorRecordResultFromCommand(result);
  record.command = DoorCommand::Open;
  record.newTravelPulses = g_door.snapshot().openTargetPulses;
  recordBusinessEvent(record);
  sendCommandResultJson(result);
#endif
}

void FarmDoorApp::handleDoorClose() {
#if ESP32BASE_ENABLE_WEB
  const DoorCommandResult result = g_door.requestClose();
  DoorRecord record;
  record.type = DoorRecordType::CommandRequested;
  record.result = doorRecordResultFromCommand(result);
  record.command = DoorCommand::Close;
  recordBusinessEvent(record);
  sendCommandResultJson(result);
#endif
}

void FarmDoorApp::handleDoorStop() {
#if ESP32BASE_ENABLE_WEB
  const DoorSnapshot snapshot = g_door.snapshot();
  const DoorCommandResult result = g_door.requestStop(snapshot.positionPulses);
  DoorRecord record;
  record.type = DoorRecordType::CommandRequested;
  record.result = doorRecordResultFromCommand(result);
  record.command = DoorCommand::Stop;
  record.oldPositionPulses = snapshot.positionPulses;
  record.newPositionPulses = g_door.snapshot().positionPulses;
  recordBusinessEvent(record);
  sendCommandResultJson(result);
#endif
}

void FarmDoorApp::handleSetPosition() {
#if ESP32BASE_ENABLE_WEB
  const DoorSnapshot before = g_door.snapshot();
  DoorCommandResult result = DoorCommandResult::InvalidArgument;
  char position[16];
  if (Esp32BaseWeb::getParam("position", position, sizeof(position))) {
    if (strcmp(position, "closed") == 0 || strcmp(position, "Closed") == 0) {
      result = g_door.markPositionClosed();
      DoorRecord record;
      record.type = DoorRecordType::PositionSet;
      record.result = doorRecordResultFromCommand(result);
      record.oldPositionPulses = before.positionPulses;
      record.newPositionPulses = g_door.snapshot().positionPulses;
      recordBusinessEvent(record);
      sendCommandResultJson(result);
      return;
    }
    if (strcmp(position, "open") == 0 || strcmp(position, "Open") == 0) {
      result = g_door.markPositionOpen();
      DoorRecord record;
      record.type = DoorRecordType::PositionSet;
      record.result = doorRecordResultFromCommand(result);
      record.oldPositionPulses = before.positionPulses;
      record.newPositionPulses = g_door.snapshot().positionPulses;
      recordBusinessEvent(record);
      sendCommandResultJson(result);
      return;
    }
    if (strcmp(position, "unknown") == 0 || strcmp(position, "Unknown") == 0) {
      result = g_door.setTrustedPosition(0, PositionTrustLevel::Untrusted);
      DoorRecord record;
      record.type = DoorRecordType::PositionSet;
      record.result = doorRecordResultFromCommand(result);
      record.oldPositionPulses = before.positionPulses;
      record.newPositionPulses = g_door.snapshot().positionPulses;
      recordBusinessEvent(record);
      sendCommandResultJson(result);
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
  result = g_door.setTrustedPosition(positionPulses, trustLevel);
  DoorRecord record;
  record.type = DoorRecordType::PositionSet;
  record.result = doorRecordResultFromCommand(result);
  record.oldPositionPulses = before.positionPulses;
  record.newPositionPulses = g_door.snapshot().positionPulses;
  recordBusinessEvent(record);
  sendCommandResultJson(result);
#endif
}

void FarmDoorApp::handleSetTravel() {
#if ESP32BASE_ENABLE_WEB
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

  bool configSaved = false;
  const int64_t oldTravelPulses = g_door.snapshot().openTargetPulses;
  const DoorCommandResult result = applyTravelUpdate(openTargetPulses, maxRunPulses, configSaved);
  DoorRecord record;
  record.type = DoorRecordType::TravelSet;
  record.result = doorRecordResultFromCommand(result);
  record.oldTravelPulses = oldTravelPulses;
  record.newTravelPulses = g_door.snapshot().openTargetPulses;
  record.deltaPulses = record.newTravelPulses - record.oldTravelPulses;
  recordBusinessEvent(record);
  sendTravelResultJson(result, configSaved);
#endif
}

void FarmDoorApp::handleAdjustTravel() {
#if ESP32BASE_ENABLE_WEB
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
  bool configSaved = false;
  const int64_t oldTravelPulses = g_door.snapshot().openTargetPulses;
  const DoorCommandResult result = applyTravelUpdate(openTargetPulses, maxRunPulses, configSaved);
  DoorRecord record;
  record.type = DoorRecordType::TravelAdjusted;
  record.result = doorRecordResultFromCommand(result);
  record.oldTravelPulses = oldTravelPulses;
  record.newTravelPulses = g_door.snapshot().openTargetPulses;
  record.deltaPulses = deltaPulses;
  recordBusinessEvent(record);
  sendTravelResultJson(result, configSaved);
#endif
}

void FarmDoorApp::handleClearFault() {
#if ESP32BASE_ENABLE_WEB
  const DoorCommandResult result = g_door.clearFault();
  DoorRecord record;
  record.type = DoorRecordType::FaultCleared;
  record.result = doorRecordResultFromCommand(result);
  recordBusinessEvent(record);
  sendCommandResultJson(result);
#endif
}
