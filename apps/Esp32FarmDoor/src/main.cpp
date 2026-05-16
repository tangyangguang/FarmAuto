#include <Arduino.h>
#include <Esp32Base.h>
#include <Esp32At24cRecordStore.h>
#include <Esp32EncodedDcMotor.h>
#include <Esp32MotorCurrentGuard.h>

namespace {

constexpr uint8_t PIN_CURRENT_ADC = 33;
constexpr uint8_t PIN_BUTTON_AUX = 36;
constexpr uint8_t PIN_BUTTON_OPEN = 39;
constexpr uint8_t PIN_BUTTON_CLOSE = 34;
constexpr uint8_t PIN_BUTTON_STOP = 35;
constexpr uint8_t PIN_ENCODER_A = 25;
constexpr uint8_t PIN_ENCODER_B = 26;
constexpr uint8_t PIN_MOTOR_IN1 = 16;
constexpr uint8_t PIN_MOTOR_IN2 = 17;
constexpr uint8_t PIN_I2C_SDA = 21;
constexpr uint8_t PIN_I2C_SCL = 22;

struct FarmDoorHardwarePins {
  uint8_t currentAdc = PIN_CURRENT_ADC;
  uint8_t buttonAux = PIN_BUTTON_AUX;
  uint8_t buttonOpen = PIN_BUTTON_OPEN;
  uint8_t buttonClose = PIN_BUTTON_CLOSE;
  uint8_t buttonStop = PIN_BUTTON_STOP;
  uint8_t encoderA = PIN_ENCODER_A;
  uint8_t encoderB = PIN_ENCODER_B;
  uint8_t motorIn1 = PIN_MOTOR_IN1;
  uint8_t motorIn2 = PIN_MOTOR_IN2;
  uint8_t i2cSda = PIN_I2C_SDA;
  uint8_t i2cScl = PIN_I2C_SCL;
};

FarmDoorHardwarePins g_pins;
Esp32EncodedDcMotor::MotorHardwareConfig g_motorHardware;
Esp32EncodedDcMotor::EncoderBackendConfig g_encoderBackend;
Esp32EncodedDcMotor::MotorKinematics g_motorKinematics;
Esp32EncodedDcMotor::MotorMotionProfile g_motorProfile;
Esp32EncodedDcMotor::MotorProtection g_motorProtection;
Esp32EncodedDcMotor::MotorStopPolicy g_motorStopPolicy;
Esp32MotorCurrentGuard::Ina240A2AnalogConfig g_currentSensor;
Esp32MotorCurrentGuard::MotorCurrentGuardConfig g_currentGuard;
Esp32At24cRecordStore::RecordStoreConfig g_recordStoreConfig;

void configureStaticDefaults() {
  g_motorHardware.driverType = Esp32EncodedDcMotor::DriverType::At8236HBridge;
  g_motorHardware.pwmFrequencyHz = 20000;
  g_motorHardware.pwmResolutionBits = 8;
  g_motorHardware.ledcChannelA = 0;
  g_motorHardware.ledcChannelB = 1;

  g_encoderBackend.backendType = Esp32EncodedDcMotor::EncoderBackendType::Pcnt;
  g_encoderBackend.pinA = g_pins.encoderA;
  g_encoderBackend.pinB = g_pins.encoderB;
  g_encoderBackend.countMode = Esp32EncodedDcMotor::CountMode::X1;

  g_motorKinematics.motorShaftPulsesPerRev = 16;
  g_motorKinematics.gearRatio = 131.0f;
  g_motorKinematics.outputPulsesPerRev = 2096;
  g_motorKinematics.countMode = Esp32EncodedDcMotor::CountMode::X1;

  g_motorProfile.speedPercent = 60;
  g_motorProfile.softStartMs = 1000;
  g_motorProfile.softStopMs = 500;
  g_motorProfile.minEffectiveSpeedPercent = 15;

  g_motorProtection.startupGraceMs = 1000;
  g_motorProtection.maxRunMs = 25000;
  g_motorProtection.maxRunPulses = 2096 * 8;

  g_motorStopPolicy.emergencyOutputMode = Esp32EncodedDcMotor::EmergencyOutputMode::Coast;

  g_currentSensor.adcPin = PIN_CURRENT_ADC;
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
}

void configureAppConfigPage() {
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

void configureBusinessShell() {
#if ESP32BASE_ENABLE_WEB
  Esp32BaseWeb::setDeviceName("Esp32FarmDoor");
  Esp32BaseWeb::setHomeMode(Esp32BaseWeb::HOME_ESP32BASE);
  Esp32BaseWeb::setSystemNavMode(Esp32BaseWeb::SYSTEM_NAV_BOTTOM);
#endif
}

}  // namespace

void setup() {
  Serial.begin(115200);
  configureStaticDefaults();

  Esp32Base::setFirmwareInfo("Esp32FarmDoor", "0.1.0");
  configureAppConfigPage();
  configureBusinessShell();

  if (!Esp32Base::begin()) {
    ESP32BASE_LOG_E("farmdoor", "Esp32Base begin failed: %s", Esp32Base::lastError());
  } else {
    ESP32BASE_LOG_I("farmdoor", "skeleton ready, ina240a2_gpio=%u enabled=%s",
                    PIN_CURRENT_ADC,
                    g_currentGuard.enabled ? "yes" : "no");
  }
}

void loop() {
  Esp32Base::handle();
}
