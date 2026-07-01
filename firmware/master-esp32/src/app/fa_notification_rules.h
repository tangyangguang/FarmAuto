#ifndef FA_NOTIFICATION_RULES_H
#define FA_NOTIFICATION_RULES_H

namespace FaNotificationConfig {
constexpr const char* NS = "fa_notify";
constexpr const char* KEY_ENABLED = "enabled";
constexpr const char* KEY_ACTION_DONE = "action_done";
constexpr const char* KEY_ACTION_FAILED = "action_failed";
constexpr const char* KEY_STATION_FAULT = "station_fault";
constexpr const char* KEY_STATION_OFFLINE = "station_offline";
constexpr const char* KEY_SCHEDULE_SKIPPED = "sched_skip";
constexpr const char* KEY_POWER_RESTORED = "power_restore";
}

#endif
