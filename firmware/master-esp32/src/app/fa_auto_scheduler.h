#ifndef FA_AUTO_SCHEDULER_H
#define FA_AUTO_SCHEDULER_H

#include "fa_device_registry.h"
#include "fa_master_action_runtime.h"
#include "fa_rs485_transport.h"

extern "C" {
#include "fa_door_service.h"
#include "fa_feed_service.h"
#include "fa_rs485_master.h"
}

namespace FaAutoScheduleConfig {
constexpr const char* NS = "fa_auto";
constexpr const char* KEY_ENABLED = "enabled";
constexpr const char* KEY_TZ_OFFSET_MIN = "tz_min";
constexpr const char* KEY_FEED_ENABLED = "feed_en";
constexpr const char* KEY_FEED_1_MIN = "feed1_min";
constexpr const char* KEY_FEED_1_AMOUNT_MG = "feed1_mg";
constexpr const char* KEY_FEED_2_MIN = "feed2_min";
constexpr const char* KEY_FEED_2_AMOUNT_MG = "feed2_mg";
constexpr const char* KEY_DOOR_ENABLED = "door_en";
constexpr const char* KEY_DOOR_OPEN_MIN = "door_open";
constexpr const char* KEY_DOOR_CLOSE_MIN = "door_close";
constexpr const char* KEY_FEED_PAUSE_UNTIL = "feed_pause";
constexpr const char* KEY_DOOR_PAUSE_UNTIL = "door_pause";
}

struct FaAutoScheduleState {
    bool time_synced = false;
    bool enabled = false;
    bool feed_enabled = false;
    bool door_enabled = false;
    uint32_t epoch_sec = 0u;
    int32_t local_day = 0;
    uint16_t local_minute = 0u;
    uint16_t feed_1_minute = 0u;
    uint32_t feed_1_amount_mg = 0u;
    uint16_t feed_2_minute = 0u;
    uint32_t feed_2_amount_mg = 0u;
    uint16_t door_open_minute = 0u;
    uint16_t door_close_minute = 0u;
    uint32_t feed_pause_until = 0u;
    uint32_t door_pause_until = 0u;
};

class FaAutoScheduler {
public:
    static constexpr uint32_t kCheckIntervalMs = 1000u;

    void begin(FaRs485Master* master,
               FaRs485Transport* transport,
               FaFeedService* feed_service,
               FaDoorService* door_service,
               FaDeviceRegistry* registry,
               FaMasterActionRuntime* action_runtime);
    void handle();

    FaAutoScheduleState snapshot() const;

private:
    bool readSchedule(FaAutoScheduleState& state) const;
    bool triggerFeed(uint8_t source_id, uint32_t amount_mg);
    bool triggerDoor(uint8_t source_id, uint8_t command);
    bool prepareDevice(uint8_t device_type, uint16_t fallback_device_id, uint8_t& station_address, uint16_t& device_id, char* device_name, size_t device_name_len);
    bool sendMotorConfig(uint8_t station_address, const FaMasterMotorConfig& motor_config, const char* stage);
    bool sendAction(uint8_t station_address, const FaMasterActionRequest& action, const char* stage);

    FaRs485Master* master_ = nullptr;
    FaRs485Transport* transport_ = nullptr;
    FaFeedService* feed_service_ = nullptr;
    FaDoorService* door_service_ = nullptr;
    FaDeviceRegistry* registry_ = nullptr;
    FaMasterActionRuntime* action_runtime_ = nullptr;
    uint32_t last_check_ms_ = 0u;
    int32_t last_day_ = -1;
    int16_t last_minute_ = -1;
};

#endif
