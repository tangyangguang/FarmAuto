#include "FarmDoorApp.h"

#include <Arduino.h>
#include <Esp32At24cRecordStore.h>
#include <Esp32Base.h>
#include <Esp32EncodedDcMotor.h>
#include <Esp32MotorCurrentGuard.h>

#include "DoorController.h"
#include "FarmDoorHardware.h"

namespace {

constexpr int64_t kDefaultOutputPulsesPerRev = 2096;
constexpr int64_t kDefaultOpenTurnsX100 = 500;
constexpr int64_t kDefaultOpenTargetPulses =
    (kDefaultOutputPulsesPerRev * kDefaultOpenTurnsX100) / 100;
constexpr int64_t kDefaultMaxRunPulses = (kDefaultOpenTargetPulses * 150) / 100;

DoorController g_door;
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
