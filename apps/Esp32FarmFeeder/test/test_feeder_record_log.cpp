#include <cassert>

#include "FeederRecordLog.h"

int main() {
  FeederRecordLog log;
  FeederRecordTime time;
  time.unixTime = 1800000000;
  time.uptimeSec = 12;
  time.bootId = 3;

  FeederRecord record;
  record.type = FeederRecordType::ManualRequested;
  record.requestedMask = 0b0011;
  log.append(record, time);

  FeederRecordSnapshot snapshot = log.snapshot();
  assert(snapshot.count == 1);
  assert(snapshot.nextSequence == 2);
  assert(snapshot.records[0].sequence == 1);
  assert(snapshot.records[0].unixTime == 1800000000);
  assert(snapshot.records[0].uptimeSec == 12);
  assert(snapshot.records[0].bootId == 3);
  assert(snapshot.records[0].requestedMask == 0b0011);

  for (uint8_t i = 0; i < kFeederRecentRecordCapacity + 2; ++i) {
    record.type = FeederRecordType::ChannelStarted;
    record.channel = i;
    log.append(record, time);
  }

  snapshot = log.snapshot();
  assert(snapshot.count == kFeederRecentRecordCapacity);
  assert(snapshot.records[0].sequence == 4);
  assert(snapshot.records[kFeederRecentRecordCapacity - 1].sequence == 19);

  assert(feederRecordResultFromCommand(FeederCommandResult::Ok) == FeederRecordResult::Ok);
  assert(feederRecordResultFromCommand(FeederCommandResult::Partial) == FeederRecordResult::Partial);
  assert(feederRecordResultFromCommand(FeederCommandResult::Busy) == FeederRecordResult::Busy);
  assert(feederRecordResultFromCommand(FeederCommandResult::Fault) == FeederRecordResult::Fault);
  assert(feederRecordResultFromCommand(FeederCommandResult::InvalidArgument) ==
         FeederRecordResult::InvalidArgument);

  return 0;
}
