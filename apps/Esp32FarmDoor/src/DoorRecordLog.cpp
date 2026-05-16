#include "DoorRecordLog.h"

DoorRecord DoorRecordLog::append(const DoorRecord& record, const DoorRecordTime& time) {
  if (snapshot_.nextSequence == 0) {
    snapshot_.nextSequence = 1;
  }

  if (snapshot_.count < kDoorRecentRecordCapacity) {
    ++snapshot_.count;
  } else {
    for (uint8_t i = 1; i < kDoorRecentRecordCapacity; ++i) {
      snapshot_.records[i - 1] = snapshot_.records[i];
    }
  }

  DoorRecord stored = record;
  stored.sequence = snapshot_.nextSequence++;
  stored.unixTime = time.unixTime;
  stored.uptimeSec = time.uptimeSec;
  stored.bootId = time.bootId;
  snapshot_.records[snapshot_.count - 1] = stored;
  return stored;
}

DoorRecordSnapshot DoorRecordLog::snapshot() const {
  return snapshot_;
}

DoorRecordResult doorRecordResultFromCommand(DoorCommandResult result) {
  switch (result) {
    case DoorCommandResult::Ok: return DoorRecordResult::Ok;
    case DoorCommandResult::Busy: return DoorRecordResult::Busy;
    case DoorCommandResult::InvalidArgument: return DoorRecordResult::InvalidArgument;
    case DoorCommandResult::PositionUntrusted: return DoorRecordResult::PositionUntrusted;
    case DoorCommandResult::FaultActive: return DoorRecordResult::FaultActive;
  }
  return DoorRecordResult::InvalidArgument;
}
