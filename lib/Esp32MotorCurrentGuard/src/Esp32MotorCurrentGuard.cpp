#include "Esp32MotorCurrentGuard.h"

namespace Esp32MotorCurrentGuard {

int32_t currentMaFromIna240A2Voltage(const Ina240A2AnalogConfig& config, int32_t voltageMv) {
  if (config.amplifierGain <= 0.0f || config.senseResistorMilliOhm <= 0.0f) {
    return 0;
  }
  const int32_t deltaMv = config.bidirectional ? (voltageMv - config.zeroOffsetMv) : voltageMv;
  const float currentMa =
      static_cast<float>(deltaMv) * 1000.0f / (config.amplifierGain * config.senseResistorMilliOhm);
  return static_cast<int32_t>(currentMa >= 0 ? currentMa + 0.5f : currentMa - 0.5f);
}

CurrentGuardResult MotorCurrentGuard::configure(const MotorCurrentGuardConfig& config) {
  if (isInvalidConfig(config)) {
    return CurrentGuardResult::InvalidConfig;
  }

  config_ = config;
  reset();
  snapshot_.warningThresholdMa = config_.warningThresholdMa;
  snapshot_.faultThresholdMa = config_.faultThresholdMa;
  snapshot_.state = config_.enabled ? CurrentGuardState::Normal : CurrentGuardState::Disabled;
  trace_.warningThresholdMa = config_.warningThresholdMa;
  trace_.faultThresholdMa = config_.faultThresholdMa;
  trace_.state = snapshot_.state;
  return CurrentGuardResult::Ok;
}

void MotorCurrentGuard::reset() {
  snapshot_ = CurrentSnapshot{};
  trace_ = CurrentTracePoint{};
  hasFilteredSample_ = false;
  firstOverThresholdMs_ = 0;

  snapshot_.warningThresholdMa = config_.warningThresholdMa;
  snapshot_.faultThresholdMa = config_.faultThresholdMa;
  snapshot_.state = config_.enabled ? CurrentGuardState::Normal : CurrentGuardState::Disabled;

  trace_.warningThresholdMa = config_.warningThresholdMa;
  trace_.faultThresholdMa = config_.faultThresholdMa;
  trace_.state = snapshot_.state;
}

CurrentGuardResult MotorCurrentGuard::update(const CurrentSample& sample, uint32_t nowMs) {
  if (!config_.enabled) {
    snapshot_.state = CurrentGuardState::Disabled;
    snapshot_.faultReason = CurrentFaultReason::None;
    updateTrace(sample);
    return CurrentGuardResult::Disabled;
  }

  if (!sample.ok || sample.sampleLost || sample.sensorStatus != SensorStatus::Ok) {
    return handleSensorFault(sample, nowMs);
  }

  const int32_t rawMa = sample.currentMa;
  if (!hasFilteredSample_) {
    snapshot_.filteredCurrentMa = rawMa;
    hasFilteredSample_ = true;
  } else {
    snapshot_.filteredCurrentMa =
        static_cast<int32_t>((config_.filterAlpha * rawMa) +
                             ((1.0f - config_.filterAlpha) * snapshot_.filteredCurrentMa));
  }

  snapshot_.rawCurrentMa = rawMa;
  snapshot_.lastSampleMs = nowMs;

  if (nowMs < config_.startupGraceMs) {
    snapshot_.state = CurrentGuardState::Normal;
    snapshot_.faultReason = CurrentFaultReason::StartupGrace;
    snapshot_.overThresholdSamples = 0;
    firstOverThresholdMs_ = 0;
    updateTrace(sample);
    return CurrentGuardResult::Ok;
  }

  const int32_t currentAbs = absMa(snapshot_.filteredCurrentMa);
  const bool overFault = config_.faultThresholdMa > 0 && currentAbs >= config_.faultThresholdMa;
  const bool overWarning =
      config_.warningThresholdMa > 0 && currentAbs >= config_.warningThresholdMa;

  if (overFault) {
    if (snapshot_.overThresholdSamples == 0) {
      firstOverThresholdMs_ = nowMs;
    }
    snapshot_.overThresholdSamples++;

    const bool samplesConfirmed = snapshot_.overThresholdSamples >= config_.confirmationSamples;
    const bool timeConfirmed =
        config_.confirmationMs == 0 || (nowMs - firstOverThresholdMs_) >= config_.confirmationMs;

    if (samplesConfirmed && timeConfirmed) {
      snapshot_.state = CurrentGuardState::Fault;
      snapshot_.faultReason = CurrentFaultReason::OverCurrent;
      if (snapshot_.faultSinceMs == 0) {
        snapshot_.faultSinceMs = nowMs;
      }
      updateTrace(sample);
      return CurrentGuardResult::FaultActive;
    }

    snapshot_.state = CurrentGuardState::Warning;
    snapshot_.faultReason = CurrentFaultReason::OverWarning;
    if (snapshot_.warningSinceMs == 0) {
      snapshot_.warningSinceMs = nowMs;
    }
    updateTrace(sample);
    return CurrentGuardResult::Ok;
  }

  snapshot_.overThresholdSamples = 0;
  firstOverThresholdMs_ = 0;

  if (overWarning) {
    snapshot_.state = CurrentGuardState::Warning;
    snapshot_.faultReason = CurrentFaultReason::OverWarning;
    if (snapshot_.warningSinceMs == 0) {
      snapshot_.warningSinceMs = nowMs;
    }
  } else {
    snapshot_.state = CurrentGuardState::Normal;
    snapshot_.faultReason = CurrentFaultReason::None;
    snapshot_.warningSinceMs = 0;
    snapshot_.faultSinceMs = 0;
  }

  updateTrace(sample);
  return CurrentGuardResult::Ok;
}

CurrentSnapshot MotorCurrentGuard::snapshot() const {
  return snapshot_;
}

CurrentTracePoint MotorCurrentGuard::latestTracePoint() const {
  return trace_;
}

int32_t MotorCurrentGuard::absMa(int32_t value) {
  return value < 0 ? -value : value;
}

bool MotorCurrentGuard::isInvalidConfig(const MotorCurrentGuardConfig& config) const {
  if (config.filterAlpha <= 0.0f || config.filterAlpha > 1.0f) {
    return true;
  }
  if (config.confirmationSamples == 0) {
    return true;
  }
  if (config.faultThresholdMa > 0 && config.warningThresholdMa > config.faultThresholdMa) {
    return true;
  }
  return false;
}

CurrentGuardResult MotorCurrentGuard::handleSensorFault(const CurrentSample& sample,
                                                        uint32_t nowMs) {
  snapshot_.lastSampleMs = nowMs;
  if (sample.sampleLost) {
    snapshot_.sampleLostCount++;
  }
  if (sample.sensorStatus == SensorStatus::AdcSaturated) {
    snapshot_.adcSaturationCount++;
  }
  snapshot_.sensorFaultCount++;

  if (config_.sensorFaultPolicy == SensorFaultPolicy::Ignore) {
    snapshot_.state = CurrentGuardState::Normal;
    snapshot_.faultReason = CurrentFaultReason::None;
    updateTrace(sample);
    return CurrentGuardResult::Ok;
  }

  if (config_.sensorFaultPolicy == SensorFaultPolicy::Warning) {
    snapshot_.state = CurrentGuardState::Warning;
    snapshot_.faultReason = CurrentFaultReason::SensorFault;
    updateTrace(sample);
    return CurrentGuardResult::Ok;
  }

  snapshot_.state = CurrentGuardState::SensorFault;
  snapshot_.faultReason = CurrentFaultReason::SensorFault;
  if (snapshot_.faultSinceMs == 0) {
    snapshot_.faultSinceMs = nowMs;
  }
  updateTrace(sample);
  return CurrentGuardResult::SensorFault;
}

void MotorCurrentGuard::updateTrace(const CurrentSample& sample) {
  trace_.timestampMs = sample.timestampMs;
  trace_.state = snapshot_.state;
  trace_.faultReason = snapshot_.faultReason;
  trace_.rawAdc = sample.rawAdc;
  trace_.voltageMv = sample.voltageMv;
  trace_.rawCurrentMa = sample.currentMa;
  trace_.filteredCurrentMa = snapshot_.filteredCurrentMa;
  trace_.warningThresholdMa = config_.warningThresholdMa;
  trace_.faultThresholdMa = config_.faultThresholdMa;
  trace_.sampleLost = sample.sampleLost;
  trace_.adcSaturated = sample.sensorStatus == SensorStatus::AdcSaturated;
}

}  // namespace Esp32MotorCurrentGuard
