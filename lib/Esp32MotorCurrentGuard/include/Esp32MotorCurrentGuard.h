#pragma once

#include <cstdint>

namespace Esp32MotorCurrentGuard {

enum class CurrentGuardResult : uint8_t {
  Ok,
  Disabled,
  InvalidConfig,
  FaultActive,
  SensorFault
};

enum class CurrentGuardState : uint8_t {
  Disabled,
  Normal,
  Warning,
  Fault,
  SensorFault
};

enum class CurrentFaultReason : uint8_t {
  None,
  OverWarning,
  OverCurrent,
  SensorFault,
  StartupGrace
};

enum class SensorStatus : uint8_t {
  Ok,
  ReadFailed,
  AdcSaturated,
  InvalidCalibration
};

enum class SensorFaultPolicy : uint8_t {
  Ignore,
  Warning,
  Fault
};

struct Ina240A2AnalogConfig {
  int8_t adcPin = -1;
  uint8_t adcResolutionBits = 12;
  uint16_t adcReferenceMv = 3300;
  uint8_t adcAttenuationDb = 11;
  float amplifierGain = 50.0f;
  float senseResistorMilliOhm = 5.0f;
  int32_t zeroOffsetMv = 0;
  bool bidirectional = true;
};

int32_t currentMaFromIna240A2Voltage(const Ina240A2AnalogConfig& config, int32_t voltageMv);

struct MotorCurrentGuardConfig {
  bool enabled = false;
  int32_t warningThresholdMa = 0;
  int32_t faultThresholdMa = 0;
  uint32_t startupGraceMs = 1000;
  uint32_t confirmationMs = 100;
  uint16_t confirmationSamples = 2;
  float filterAlpha = 0.2f;
  SensorFaultPolicy sensorFaultPolicy = SensorFaultPolicy::Fault;
};

struct CurrentSample {
  uint32_t timestampMs = 0;
  uint32_t sequence = 0;
  uint16_t rawAdc = 0;
  int32_t voltageMv = 0;
  int32_t currentMa = 0;
  SensorStatus sensorStatus = SensorStatus::Ok;
  bool ok = true;
  bool sampleLost = false;
};

struct CurrentTracePoint {
  uint32_t timestampMs = 0;
  CurrentGuardState state = CurrentGuardState::Disabled;
  CurrentFaultReason faultReason = CurrentFaultReason::None;
  uint16_t rawAdc = 0;
  int32_t voltageMv = 0;
  int32_t rawCurrentMa = 0;
  int32_t filteredCurrentMa = 0;
  int32_t warningThresholdMa = 0;
  int32_t faultThresholdMa = 0;
  bool sampleLost = false;
  bool adcSaturated = false;
};

struct CurrentSnapshot {
  CurrentGuardState state = CurrentGuardState::Disabled;
  CurrentFaultReason faultReason = CurrentFaultReason::None;
  int32_t rawCurrentMa = 0;
  int32_t filteredCurrentMa = 0;
  int32_t warningThresholdMa = 0;
  int32_t faultThresholdMa = 0;
  uint32_t lastSampleMs = 0;
  uint32_t warningSinceMs = 0;
  uint32_t faultSinceMs = 0;
  uint16_t overThresholdSamples = 0;
  uint32_t adcSaturationCount = 0;
  uint32_t sensorFaultCount = 0;
  uint32_t sampleLostCount = 0;
};

class MotorCurrentGuard {
 public:
  CurrentGuardResult configure(const MotorCurrentGuardConfig& config);
  void reset();

  CurrentGuardResult update(const CurrentSample& sample, uint32_t nowMs);

  CurrentSnapshot snapshot() const;
  CurrentTracePoint latestTracePoint() const;

 private:
  static int32_t absMa(int32_t value);
  bool isInvalidConfig(const MotorCurrentGuardConfig& config) const;
  CurrentGuardResult handleSensorFault(const CurrentSample& sample, uint32_t nowMs);
  void updateTrace(const CurrentSample& sample);

  MotorCurrentGuardConfig config_;
  CurrentSnapshot snapshot_;
  CurrentTracePoint trace_;
  bool hasFilteredSample_ = false;
  uint32_t firstOverThresholdMs_ = 0;
};

}  // namespace Esp32MotorCurrentGuard
