#include <cassert>

#include "FeederSchedule.h"

int main() {
  FeederScheduleService schedules;
  schedules.beginDay(20260516);

  FeederPlanConfig morning;
  morning.enabled = true;
  morning.timeConfigured = true;
  morning.timeMinutes = 7 * 60 + 30;
  morning.channelMask = 0b0011;
  morning.targets[0].mode = FeederTargetMode::Grams;
  morning.targets[0].targetGramsX100 = 7000;
  morning.targets[1].mode = FeederTargetMode::Revolutions;
  morning.targets[1].targetRevolutionsX100 = 100;

  FeederPlanConfig evening = morning;
  evening.timeMinutes = 18 * 60;
  evening.channelMask = 0b0001;

  FeederPlanConfig draft;
  draft.enabled = false;
  assert(schedules.addPlan(draft).result == FeederScheduleResult::Ok);
  assert(schedules.snapshot().planCount == 1);
  assert(schedules.snapshot().plans[0].config.planId == 1);
  assert(schedules.deletePlan(1) == FeederScheduleResult::Ok);

  assert(schedules.addPlan(morning).result == FeederScheduleResult::Ok);
  assert(schedules.addPlan(evening).result == FeederScheduleResult::Ok);
  assert(schedules.snapshot().planCount == 2);
  assert(schedules.nextPlan(7 * 60).config.planId == 1);

  evening.timeMinutes = 19 * 60;
  assert(schedules.updatePlan(2, evening) == FeederScheduleResult::Ok);
  assert(schedules.snapshot().plans[1].config.timeMinutes == 19 * 60);

  FeederScheduleTick tick = schedules.evaluate(7 * 60 + 30, true);
  assert(tick.action == FeederScheduleAction::TriggerPlan);
  assert(tick.planId == 1);
  assert(schedules.markAttempted(1) == FeederScheduleResult::Ok);
  assert(schedules.markExecuted(1) == FeederScheduleResult::Ok);
  assert(schedules.snapshot().plans[0].scheduleAttemptedToday);
  assert(schedules.snapshot().plans[0].todayExecuted);

  assert(schedules.skipToday(2) == FeederScheduleResult::Ok);
  assert(schedules.evaluate(19 * 60, true).action == FeederScheduleAction::NoAction);

  schedules.beginDay(20260517);
  assert(!schedules.snapshot().plans[0].todayExecuted);
  assert(!schedules.snapshot().plans[1].skipToday);

  assert(schedules.evaluate(19 * 60 + 1, true).action == FeederScheduleAction::MarkMissed);
  assert(schedules.snapshot().plans[0].scheduleMissedToday);
  assert(schedules.evaluate(19 * 60 + 1, true).action == FeederScheduleAction::MarkMissed);
  assert(schedules.snapshot().plans[1].scheduleMissedToday);

  assert(schedules.skipToday(2) == FeederScheduleResult::Ok);
  schedules.clearToday();
  assert(!schedules.snapshot().plans[0].scheduleMissedToday);
  assert(!schedules.snapshot().plans[1].scheduleMissedToday);
  assert(!schedules.snapshot().plans[1].skipToday);

  assert(schedules.skipToday(99) == FeederScheduleResult::NotFound);
  assert(schedules.deletePlan(1) == FeederScheduleResult::Ok);
  assert(schedules.snapshot().planCount == 1);
  assert(schedules.snapshot().plans[0].config.planId == 2);

  return 0;
}
