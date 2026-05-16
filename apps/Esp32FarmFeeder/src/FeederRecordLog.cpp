#include "FeederRecordLog.h"

void FeederRecordLog::append(const FeederRecord& record, const FeederRecordTime& time) {
  if (snapshot_.nextSequence == 0) {
    snapshot_.nextSequence = 1;
  }

  if (snapshot_.count < kFeederRecentRecordCapacity) {
    ++snapshot_.count;
  } else {
    for (uint8_t i = 1; i < kFeederRecentRecordCapacity; ++i) {
      snapshot_.records[i - 1] = snapshot_.records[i];
    }
  }

  FeederRecord stored = record;
  stored.sequence = snapshot_.nextSequence++;
  stored.unixTime = time.unixTime;
  stored.uptimeSec = time.uptimeSec;
  stored.bootId = time.bootId;
  snapshot_.records[snapshot_.count - 1] = stored;
}

FeederRecordSnapshot FeederRecordLog::snapshot() const {
  return snapshot_;
}

FeederRecordResult feederRecordResultFromCommand(FeederCommandResult result) {
  switch (result) {
    case FeederCommandResult::Ok: return FeederRecordResult::Ok;
    case FeederCommandResult::Partial: return FeederRecordResult::Partial;
    case FeederCommandResult::Busy: return FeederRecordResult::Busy;
    case FeederCommandResult::Fault: return FeederRecordResult::Fault;
    case FeederCommandResult::NotConfigured: return FeederRecordResult::Skipped;
    case FeederCommandResult::InvalidArgument: return FeederRecordResult::InvalidArgument;
  }
  return FeederRecordResult::InvalidArgument;
}
