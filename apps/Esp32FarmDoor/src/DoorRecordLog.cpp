#include "DoorRecordLog.h"

#include <cstring>

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

bool doorRecordTypeFromName(const char* name, DoorRecordType& out) {
  if (name == nullptr || name[0] == '\0') {
    return false;
  }
  if (std::strcmp(name, "DoorCommandRequested") == 0 ||
      std::strcmp(name, "CommandRequested") == 0) {
    out = DoorRecordType::CommandRequested;
    return true;
  }
  if (std::strcmp(name, "DoorPositionSet") == 0 || std::strcmp(name, "PositionSet") == 0) {
    out = DoorRecordType::PositionSet;
    return true;
  }
  if (std::strcmp(name, "DoorTravelSet") == 0 || std::strcmp(name, "TravelSet") == 0) {
    out = DoorRecordType::TravelSet;
    return true;
  }
  if (std::strcmp(name, "DoorTravelAdjusted") == 0 ||
      std::strcmp(name, "TravelAdjusted") == 0) {
    out = DoorRecordType::TravelAdjusted;
    return true;
  }
  if (std::strcmp(name, "DoorFaultCleared") == 0 || std::strcmp(name, "FaultCleared") == 0) {
    out = DoorRecordType::FaultCleared;
    return true;
  }
  return false;
}
