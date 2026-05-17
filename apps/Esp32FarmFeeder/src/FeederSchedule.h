#pragma once

#include <cstdint>

#include "FeederController.h"

static constexpr uint8_t kFeederMaxPlans = 6;

enum class FeederTargetMode : uint8_t {
  None,
  Grams,
  Revolutions
};

enum class FeederScheduleResult : uint8_t {
  Ok,
  Full,
  NotFound,
  InvalidArgument
};

enum class FeederScheduleAction : uint8_t {
  NoAction,
  TriggerPlan,
  MarkMissed
};

struct FeederChannelTarget {
  FeederTargetMode mode = FeederTargetMode::None;
  int32_t targetGramsX100 = 0;
  int32_t targetRevolutionsX100 = 0;
};

struct FeederPlanConfig {
  uint8_t planId = 0;
  bool enabled = false;
  bool timeConfigured = false;
  uint16_t timeMinutes = 0;
  uint8_t channelMask = 0;
  FeederChannelTarget targets[kFeederMaxChannels];
};

struct FeederPlanState {
  FeederPlanConfig config;
  uint32_t skipServiceDate = 0;
  bool skipToday = false;
  bool scheduleAttemptedToday = false;
  bool todayExecuted = false;
  bool scheduleMissedToday = false;
};

struct FeederScheduleSnapshot {
  uint32_t serviceDate = 0;
  uint8_t planCount = 0;
  FeederPlanState plans[kFeederMaxPlans];
};

struct FeederScheduleMutation {
  FeederScheduleResult result = FeederScheduleResult::InvalidArgument;
  uint8_t planId = 0;
};

struct FeederScheduleTick {
  FeederScheduleAction action = FeederScheduleAction::NoAction;
  uint8_t planId = 0;
};

class FeederScheduleService {
 public:
  void beginDay(uint32_t serviceDate);
  FeederScheduleMutation addPlan(const FeederPlanConfig& config);
  FeederScheduleResult updatePlan(uint8_t planId, const FeederPlanConfig& config);
  FeederScheduleResult deletePlan(uint8_t planId);
  FeederScheduleResult skipToday(uint8_t planId);
  FeederScheduleResult cancelSkipToday(uint8_t planId);
  FeederScheduleResult skipOccurrence(uint8_t planId, uint32_t serviceDate);
  FeederScheduleResult cancelSkipOccurrence(uint8_t planId, uint32_t serviceDate);
  FeederScheduleResult markAttempted(uint8_t planId);
  FeederScheduleResult markExecuted(uint8_t planId);
  void clearToday();
  FeederScheduleTick evaluate(uint16_t currentMinutes, bool timeValid);
  FeederPlanState nextPlan(uint16_t currentMinutes) const;
  FeederScheduleSnapshot snapshot() const;
  FeederScheduleResult restore(const FeederScheduleSnapshot& snapshot);

 private:
  int findPlan(uint8_t planId) const;
  uint8_t allocatePlanId() const;
  bool planCanRunToday(const FeederPlanState& plan) const;
  static bool hasRunnableTarget(const FeederPlanConfig& config);

  FeederScheduleSnapshot snapshot_;
};
