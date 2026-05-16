#include <cassert>
#include <cstdint>

#include "Esp32MotorCurrentGuard.h"

using namespace Esp32MotorCurrentGuard;

int main() {
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

  return 0;
}
