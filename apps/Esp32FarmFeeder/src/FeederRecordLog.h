#pragma once

#include <cstdint>

#include "FeederController.h"

static constexpr uint8_t kFeederRecentRecordCapacity = 16;

enum class FeederRecordType : uint8_t {
  ManualRequested,
  ScheduleTriggered,
  ScheduleMissed,
  ChannelStarted,
  ChannelStopped,
  BatchCompleted
};

enum class FeederRecordResult : uint8_t {
  Ok,
  Partial,
  Busy,
  Fault,
  Skipped,
  InvalidArgument
};

struct FeederRecord {
  uint32_t sequence = 0;
  uint32_t unixTime = 0;
  uint32_t uptimeSec = 0;
  uint32_t bootId = 0;
  FeederRecordType type = FeederRecordType::ManualRequested;
  FeederRecordResult result = FeederRecordResult::Ok;
  uint8_t planId = 0;
  uint8_t channel = 255;
  uint8_t requestedMask = 0;
  uint8_t successMask = 0;
  uint8_t busyMask = 0;
  uint8_t faultMask = 0;
  uint8_t skippedMask = 0;
  int32_t targetPulses = 0;
  int32_t estimatedGramsX100 = 0;
  int32_t actualPulses = 0;
};

struct FeederRecordSnapshot {
  uint32_t nextSequence = 1;
  uint8_t count = 0;
  FeederRecord records[kFeederRecentRecordCapacity];
};

struct FeederRecordTime {
  uint32_t unixTime = 0;
  uint32_t uptimeSec = 0;
  uint32_t bootId = 0;
};

class FeederRecordLog {
 public:
  FeederRecord append(const FeederRecord& record, const FeederRecordTime& time);
  FeederRecordSnapshot snapshot() const;

 private:
  FeederRecordSnapshot snapshot_;
};

FeederRecordResult feederRecordResultFromCommand(FeederCommandResult result);
