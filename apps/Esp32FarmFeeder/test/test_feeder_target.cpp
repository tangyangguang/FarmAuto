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

  FeederBucketSnapshot buckets;
  FeederTargetSnapshot targetSnapshot;

  buckets.channels[0].baseInfo = info;
  buckets.channels[0].baseInfo.enabled = true;
  buckets.channels[0].baseInfo.gramsPerRevX100 = 7000;
  targetSnapshot.channels[0] = grams;

  buckets.channels[1].baseInfo = buckets.channels[0].baseInfo;
  buckets.channels[1].baseInfo.gramsPerRevX100 = 0;
  targetSnapshot.channels[1] = grams;

  buckets.channels[2].baseInfo = buckets.channels[0].baseInfo;
  buckets.channels[2].baseInfo.enabled = false;
  targetSnapshot.channels[2] = grams;

  FeederTargetBatch batch = resolveFeederTargetsForMask(buckets, targetSnapshot, 0b0111);
  assert(batch.okMask == 0b0001);
  assert(batch.notCalibratedMask == 0b0010);
  assert(batch.invalidMask == 0b0100);
  assert(batch.channels[0].targetPulses == 4320);
  assert(batch.channels[0].estimatedGramsX100 == 7000);

  return 0;
}
