#pragma once

#include <cstdint>

#include "DoorController.h"

static constexpr uint8_t kDoorRecentRecordCapacity = 16;

enum class DoorRecordType : uint8_t {
  CommandRequested,
  PositionSet,
  TravelSet,
  TravelAdjusted,
  FaultCleared
};

enum class DoorRecordResult : uint8_t {
  Ok,
  Busy,
  InvalidArgument,
  PositionUntrusted,
  FaultActive
};

struct DoorRecord {
  uint32_t sequence = 0;
  uint32_t unixTime = 0;
  uint32_t uptimeSec = 0;
  uint32_t bootId = 0;
  DoorRecordType type = DoorRecordType::CommandRequested;
  DoorRecordResult result = DoorRecordResult::Ok;
  DoorCommand command = DoorCommand::None;
  int64_t oldPositionPulses = 0;
  int64_t newPositionPulses = 0;
  int64_t oldTravelPulses = 0;
  int64_t newTravelPulses = 0;
  int64_t deltaPulses = 0;
};

struct DoorRecordSnapshot {
  uint32_t nextSequence = 1;
  uint8_t count = 0;
  DoorRecord records[kDoorRecentRecordCapacity];
};

struct DoorRecordTime {
  uint32_t unixTime = 0;
  uint32_t uptimeSec = 0;
  uint32_t bootId = 0;
};

class DoorRecordLog {
 public:
  DoorRecord append(const DoorRecord& record, const DoorRecordTime& time);
  DoorRecordSnapshot snapshot() const;

 private:
  DoorRecordSnapshot snapshot_;
};

DoorRecordResult doorRecordResultFromCommand(DoorCommandResult result);
bool doorRecordTypeFromName(const char* name, DoorRecordType& out);
