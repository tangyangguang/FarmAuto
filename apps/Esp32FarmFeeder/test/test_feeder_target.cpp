#include <cassert>

#include "FeederTarget.h"

int main() {
  FeederChannelBaseInfo info;
  info.enabled = true;
  info.outputPulsesPerRev = 4320;
  info.gramsPerRevX100 = 7000;
  info.capacityGramsX100 = 500000;

  FeederTargetRequest grams;
  grams.mode = FeederTargetMode::Grams;
  grams.targetGramsX100 = 7000;
  FeederResolvedTarget resolved = resolveFeederTarget(info, grams);
  assert(resolved.result == FeederTargetResult::Ok);
  assert(resolved.targetPulses == 4320);
  assert(resolved.estimatedGramsX100 == 7000);

  FeederTargetRequest revolutions;
  revolutions.mode = FeederTargetMode::Revolutions;
  revolutions.targetRevolutionsX100 = 150;
  resolved = resolveFeederTarget(info, revolutions);
  assert(resolved.result == FeederTargetResult::Ok);
  assert(resolved.targetPulses == 6480);
  assert(resolved.estimatedGramsX100 == 10500);

  info.gramsPerRevX100 = 0;
  resolved = resolveFeederTarget(info, grams);
  assert(resolved.result == FeederTargetResult::NotCalibrated);

  FeederTargetService targets;
  assert(targets.setTarget(0, grams) == FeederTargetResult::Ok);
  assert(targets.snapshot().channels[0].mode == FeederTargetMode::Grams);
  assert(targets.snapshot().channels[0].targetGramsX100 == 7000);

  assert(targets.setTarget(0, revolutions) == FeederTargetResult::Ok);
  assert(targets.snapshot().channels[0].mode == FeederTargetMode::Revolutions);
  assert(targets.snapshot().channels[0].targetGramsX100 == 7000);
  assert(targets.snapshot().channels[0].targetRevolutionsX100 == 150);

  assert(targets.setTarget(4, grams) == FeederTargetResult::InvalidArgument);

  return 0;
}
