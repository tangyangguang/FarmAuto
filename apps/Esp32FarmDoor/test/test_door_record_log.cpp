#include <cassert>

#include "DoorRecordLog.h"

int main() {
  DoorRecordLog log;
  DoorRecordTime time;
  time.unixTime = 1778800000;
  time.uptimeSec = 12;
  time.bootId = 3;

  DoorRecord record;
  record.type = DoorRecordType::CommandRequested;
  record.command = DoorCommand::Open;
  record.result = DoorRecordResult::Ok;

  const DoorRecord stored = log.append(record, time);
  assert(stored.sequence == 1);
  assert(stored.unixTime == 1778800000);
  assert(stored.command == DoorCommand::Open);

  DoorRecordSnapshot snapshot = log.snapshot();
  assert(snapshot.count == 1);
  assert(snapshot.records[0].bootId == 3);

  for (uint8_t i = 0; i < kDoorRecentRecordCapacity + 2; ++i) {
    log.append(record, time);
  }
  snapshot = log.snapshot();
  assert(snapshot.count == kDoorRecentRecordCapacity);
  assert(snapshot.records[0].sequence == 4);

  assert(doorRecordResultFromCommand(DoorCommandResult::Busy) == DoorRecordResult::Busy);
  assert(doorRecordResultFromCommand(DoorCommandResult::PositionUntrusted) ==
         DoorRecordResult::PositionUntrusted);

  DoorRecordType type = DoorRecordType::CommandRequested;
  assert(doorRecordTypeFromName("DoorTravelSet", type));
  assert(type == DoorRecordType::TravelSet);
  assert(doorRecordTypeFromName("TravelAdjusted", type));
  assert(type == DoorRecordType::TravelAdjusted);
  assert(!doorRecordTypeFromName("UnknownEvent", type));

  return 0;
}
