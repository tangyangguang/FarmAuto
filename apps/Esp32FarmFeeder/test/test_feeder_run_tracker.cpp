#include <cassert>

#include "FeederRunTracker.h"

int main() {
  FeederTargetBatch targets;
  targets.okMask = 0b0011;
  targets.channels[0].targetPulses = 4320;
  targets.channels[0].estimatedGramsX100 = 7000;
  targets.channels[1].targetPulses = 2160;
  targets.channels[1].estimatedGramsX100 = 3500;

  FeederRunTracker tracker;
  tracker.start(0b0001, FeederRunSource::Manual, targets);
  FeederRunSnapshot snapshot = tracker.snapshot();
  assert(snapshot.channels[0].active);
  assert(snapshot.channels[0].source == FeederRunSource::Manual);
  assert(snapshot.channels[0].targetPulses == 4320);
  assert(snapshot.channels[0].estimatedGramsX100 == 7000);
  assert(!snapshot.channels[1].active);

  tracker.start(0b0010, FeederRunSource::Schedule, targets);
  snapshot = tracker.snapshot();
  assert(snapshot.channels[0].active);
  assert(snapshot.channels[1].active);
  assert(snapshot.channels[1].source == FeederRunSource::Schedule);
  assert(snapshot.channels[1].targetPulses == 2160);

  tracker.stop(0b0001);
  snapshot = tracker.snapshot();
  assert(!snapshot.channels[0].active);
  assert(snapshot.channels[1].active);

  tracker.stopAll();
  snapshot = tracker.snapshot();
  assert(!snapshot.channels[1].active);

  return 0;
}
