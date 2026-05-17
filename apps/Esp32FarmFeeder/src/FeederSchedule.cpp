#include "FeederSchedule.h"

void FeederScheduleService::beginDay(uint32_t serviceDate) {
  snapshot_.serviceDate = serviceDate;
  for (uint8_t i = 0; i < snapshot_.planCount; ++i) {
    snapshot_.plans[i].skipToday = false;
    snapshot_.plans[i].scheduleAttemptedToday = false;
    snapshot_.plans[i].todayExecuted = false;
    snapshot_.plans[i].scheduleMissedToday = false;
  }
}

FeederScheduleMutation FeederScheduleService::addPlan(const FeederPlanConfig& config) {
  FeederScheduleMutation mutation;
  if (snapshot_.planCount >= kFeederMaxPlans) {
    mutation.result = FeederScheduleResult::Full;
    return mutation;
  }
  if (!hasRunnableTarget(config)) {
    mutation.result = FeederScheduleResult::InvalidArgument;
    return mutation;
  }

  FeederPlanState& plan = snapshot_.plans[snapshot_.planCount++];
  plan = FeederPlanState{};
  plan.config = config;
  plan.config.planId = config.planId == 0 ? allocatePlanId() : config.planId;
  mutation.result = FeederScheduleResult::Ok;
  mutation.planId = plan.config.planId;
  return mutation;
}

FeederScheduleResult FeederScheduleService::updatePlan(uint8_t planId, const FeederPlanConfig& config) {
  const int index = findPlan(planId);
  if (index < 0) {
    return FeederScheduleResult::NotFound;
  }
  if (!hasRunnableTarget(config)) {
    return FeederScheduleResult::InvalidArgument;
  }

  FeederPlanConfig updated = config;
  updated.planId = planId;
  snapshot_.plans[index].config = updated;
  return FeederScheduleResult::Ok;
}

FeederScheduleResult FeederScheduleService::deletePlan(uint8_t planId) {
  const int index = findPlan(planId);
  if (index < 0) {
    return FeederScheduleResult::NotFound;
  }
  for (uint8_t i = static_cast<uint8_t>(index); i + 1 < snapshot_.planCount; ++i) {
    snapshot_.plans[i] = snapshot_.plans[i + 1];
  }
  --snapshot_.planCount;
  snapshot_.plans[snapshot_.planCount] = FeederPlanState{};
  return FeederScheduleResult::Ok;
}

FeederScheduleResult FeederScheduleService::skipToday(uint8_t planId) {
  const int index = findPlan(planId);
  if (index < 0) {
    return FeederScheduleResult::NotFound;
  }
  snapshot_.plans[index].skipToday = true;
  return FeederScheduleResult::Ok;
}

FeederScheduleResult FeederScheduleService::cancelSkipToday(uint8_t planId) {
  const int index = findPlan(planId);
  if (index < 0) {
    return FeederScheduleResult::NotFound;
  }
  snapshot_.plans[index].skipToday = false;
  return FeederScheduleResult::Ok;
}

FeederScheduleResult FeederScheduleService::markAttempted(uint8_t planId) {
  const int index = findPlan(planId);
  if (index < 0) {
    return FeederScheduleResult::NotFound;
  }
  snapshot_.plans[index].scheduleAttemptedToday = true;
  snapshot_.plans[index].scheduleMissedToday = false;
  return FeederScheduleResult::Ok;
}

FeederScheduleResult FeederScheduleService::markExecuted(uint8_t planId) {
  const int index = findPlan(planId);
  if (index < 0) {
    return FeederScheduleResult::NotFound;
  }
  snapshot_.plans[index].scheduleAttemptedToday = true;
  snapshot_.plans[index].todayExecuted = true;
  snapshot_.plans[index].scheduleMissedToday = false;
  return FeederScheduleResult::Ok;
}

void FeederScheduleService::clearToday() {
  for (uint8_t i = 0; i < snapshot_.planCount; ++i) {
    snapshot_.plans[i].skipToday = false;
    snapshot_.plans[i].scheduleAttemptedToday = false;
    snapshot_.plans[i].todayExecuted = false;
    snapshot_.plans[i].scheduleMissedToday = false;
  }
}

FeederScheduleTick FeederScheduleService::evaluate(uint16_t currentMinutes, bool timeValid) {
  FeederScheduleTick tick;
  if (!timeValid) {
    return tick;
  }

  for (uint8_t i = 0; i < snapshot_.planCount; ++i) {
    FeederPlanState& plan = snapshot_.plans[i];
    if (!planCanRunToday(plan)) {
      continue;
    }
    if (currentMinutes == plan.config.timeMinutes) {
      tick.action = FeederScheduleAction::TriggerPlan;
      tick.planId = plan.config.planId;
      return tick;
    }
    if (currentMinutes > plan.config.timeMinutes) {
      plan.scheduleMissedToday = true;
      if (tick.action == FeederScheduleAction::NoAction) {
        tick.action = FeederScheduleAction::MarkMissed;
        tick.planId = plan.config.planId;
      }
    }
  }
  return tick;
}

FeederPlanState FeederScheduleService::nextPlan(uint16_t currentMinutes) const {
  FeederPlanState next;
  bool found = false;
  for (uint8_t i = 0; i < snapshot_.planCount; ++i) {
    const FeederPlanState& plan = snapshot_.plans[i];
    if (!planCanRunToday(plan) || plan.config.timeMinutes < currentMinutes) {
      continue;
    }
    if (!found || plan.config.timeMinutes < next.config.timeMinutes) {
      next = plan;
      found = true;
    }
  }
  return next;
}

FeederScheduleSnapshot FeederScheduleService::snapshot() const {
  return snapshot_;
}

FeederScheduleResult FeederScheduleService::restore(const FeederScheduleSnapshot& snapshot) {
  if (snapshot.planCount > kFeederMaxPlans) {
    return FeederScheduleResult::InvalidArgument;
  }
  for (uint8_t i = 0; i < snapshot.planCount; ++i) {
    const FeederPlanState& plan = snapshot.plans[i];
    if (plan.config.planId == 0 || !hasRunnableTarget(plan.config)) {
      return FeederScheduleResult::InvalidArgument;
    }
    for (uint8_t j = static_cast<uint8_t>(i + 1); j < snapshot.planCount; ++j) {
      if (snapshot.plans[j].config.planId == plan.config.planId) {
        return FeederScheduleResult::InvalidArgument;
      }
    }
  }
  snapshot_ = snapshot;
  return FeederScheduleResult::Ok;
}

int FeederScheduleService::findPlan(uint8_t planId) const {
  for (uint8_t i = 0; i < snapshot_.planCount; ++i) {
    if (snapshot_.plans[i].config.planId == planId) {
      return i;
    }
  }
  return -1;
}

uint8_t FeederScheduleService::allocatePlanId() const {
  uint8_t maxId = 0;
  for (uint8_t i = 0; i < snapshot_.planCount; ++i) {
    if (snapshot_.plans[i].config.planId > maxId) {
      maxId = snapshot_.plans[i].config.planId;
    }
  }
  return static_cast<uint8_t>(maxId + 1);
}

bool FeederScheduleService::planCanRunToday(const FeederPlanState& plan) const {
  return plan.config.enabled && plan.config.timeConfigured && plan.config.channelMask != 0 &&
         !plan.skipToday && !plan.scheduleAttemptedToday && !plan.todayExecuted &&
         !plan.scheduleMissedToday;
}

bool FeederScheduleService::hasRunnableTarget(const FeederPlanConfig& config) {
  if (!config.enabled) {
    return !config.timeConfigured || config.timeMinutes < 24 * 60;
  }
  if (config.channelMask == 0 || !config.timeConfigured || config.timeMinutes >= 24 * 60) {
    return false;
  }
  for (uint8_t i = 0; i < kFeederMaxChannels; ++i) {
    const uint8_t bit = static_cast<uint8_t>(1U << i);
    if ((config.channelMask & bit) == 0) {
      continue;
    }
    const FeederChannelTarget& target = config.targets[i];
    if (target.mode == FeederTargetMode::Grams && target.targetGramsX100 > 0) {
      continue;
    }
    if (target.mode == FeederTargetMode::Revolutions && target.targetRevolutionsX100 > 0) {
      continue;
    }
    return false;
  }
  return true;
}
