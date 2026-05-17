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

  assert(tracker.updateActualPulses(1, 1080));
  snapshot = tracker.snapshot();
  assert(snapshot.channels[1].actualPulses == 1080);
  assert(!tracker.updateActualPulses(1, -1));
  assert(!tracker.updateActualPulses(3, 100));

  FeederRunChannel completed;
  assert(tracker.finish(1, 2160, completed));
  assert(completed.active);
  assert(completed.source == FeederRunSource::Schedule);
  assert(completed.targetPulses == 2160);
  assert(completed.estimatedGramsX100 == 3500);
  assert(completed.actualPulses == 2160);
  snapshot = tracker.snapshot();
  assert(!snapshot.channels[1].active);

  tracker.stop(0b0001);
  snapshot = tracker.snapshot();
  assert(!snapshot.channels[0].active);
  assert(!snapshot.channels[1].active);

  assert(!tracker.finish(1, 1, completed));

  tracker.start(0b0010, FeederRunSource::Schedule, targets);
  tracker.stopAll();
  snapshot = tracker.snapshot();
  assert(!snapshot.channels[1].active);

  return 0;
}
