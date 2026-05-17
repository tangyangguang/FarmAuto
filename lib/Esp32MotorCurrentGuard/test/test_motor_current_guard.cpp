#include <cassert>
#include <cstdint>

#include "Esp32MotorCurrentGuard.h"

using namespace Esp32MotorCurrentGuard;

int main() {
  Ina240A2AnalogConfig inaConfig;
  inaConfig.amplifierGain = 50.0f;
  inaConfig.senseResistorMilliOhm = 5.0f;
  inaConfig.zeroOffsetMv = 1650;
  assert(currentMaFromIna240A2Voltage(inaConfig, 1900) == 1000);
  assert(currentMaFromIna240A2Voltage(inaConfig, 1400) == -1000);

  MotorCurrentGuard guard;

  MotorCurrentGuardConfig config;
  config.enabled = true;
  config.warningThresholdMa = 700;
  config.faultThresholdMa = 1000;
  config.startupGraceMs = 0;
  config.confirmationSamples = 2;
  config.confirmationMs = 0;
  config.filterAlpha = 1.0f;

  assert(guard.configure(config) == CurrentGuardResult::Ok);

  CurrentSample normal;
  normal.timestampMs = 10;
  normal.sequence = 1;
  normal.currentMa = 500;
  assert(guard.update(normal, 10) == CurrentGuardResult::Ok);
  assert(guard.snapshot().state == CurrentGuardState::Normal);

  CurrentSample over;
  over.timestampMs = 20;
  over.sequence = 2;
  over.currentMa = 1200;
  assert(guard.update(over, 20) == CurrentGuardResult::Ok);
  assert(guard.snapshot().state == CurrentGuardState::Warning);

  over.timestampMs = 30;
  over.sequence = 3;
  assert(guard.update(over, 30) == CurrentGuardResult::FaultActive);
  assert(guard.snapshot().state == CurrentGuardState::Fault);
  assert(guard.latestTracePoint().faultReason == CurrentFaultReason::OverCurrent);

  normal.timestampMs = 40;
  normal.sequence = 4;
  assert(guard.update(normal, 40) == CurrentGuardResult::FaultActive);
  assert(guard.snapshot().state == CurrentGuardState::Fault);

  guard.reset();
  assert(guard.snapshot().state == CurrentGuardState::Normal);

  MotorCurrentGuard delayedGuard;
  config.startupGraceMs = 1000;
  config.confirmationSamples = 1;
  assert(delayedGuard.configure(config) == CurrentGuardResult::Ok);
  CurrentSample delayedOver;
  delayedOver.timestampMs = 60000;
  delayedOver.sequence = 1;
  delayedOver.currentMa = 1200;
  assert(delayedGuard.update(delayedOver, 60000) == CurrentGuardResult::Ok);
  assert(delayedGuard.snapshot().state == CurrentGuardState::Normal);
  assert(delayedGuard.snapshot().faultReason == CurrentFaultReason::StartupGrace);
  delayedOver.timestampMs = 61101;
  delayedOver.sequence = 2;
  assert(delayedGuard.update(delayedOver, 61101) == CurrentGuardResult::FaultActive);
  assert(delayedGuard.snapshot().state == CurrentGuardState::Fault);

  return 0;
}
