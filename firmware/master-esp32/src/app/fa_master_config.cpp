#include "fa_master_config.h"

#include <Esp32Base.h>

namespace {

uint32_t readActionId(const char* key, uint32_t fallback) {
    const int32_t raw = Esp32BaseConfig::getInt(FaActionIdConfig::NS, key, static_cast<int32_t>(fallback));
    return raw > 0 ? static_cast<uint32_t>(raw) : fallback;
}

bool saveActionId(const char* key, uint32_t next_action_id) {
    if (next_action_id == 0u || next_action_id > 0x7FFFFFFFul) {
        return false;
    }
    return Esp32BaseConfig::setInt(FaActionIdConfig::NS, key, static_cast<int32_t>(next_action_id));
}

}  // namespace

FaFeedDeviceConfig fa_master_read_feed_config(void) {
    FaFeedDeviceConfig config;
    config.station_address = static_cast<uint8_t>(Esp32BaseConfig::getInt(kNs, kStationAddress, 1));
    config.config_version = 1u;
    config.pulses_per_turn = static_cast<uint32_t>(Esp32BaseConfig::getInt(kNs, kPulsesPerTurn, 4320));
    config.grams_per_turn_mg = static_cast<uint32_t>(Esp32BaseConfig::getInt(kNs, kGramsPerTurnMg, 8000));
    config.feed_direction = static_cast<int8_t>(Esp32BaseConfig::getInt(kNs, kDirection, 1));
    config.speed_permille = static_cast<uint16_t>(Esp32BaseConfig::getInt(kNs, kSpeedPermille, 800));
    config.accel_ms = 0u;
    config.decel_ms = 0u;
    config.over_current_ma = static_cast<uint16_t>(Esp32BaseConfig::getInt(kNs, kOverCurrentMa, 2000));
    config.over_current_hold_ms = 100u;
    config.stall_detect_ms = 500u;
    config.stall_min_delta_pulses = 10u;
    config.max_run_ms = static_cast<uint32_t>(Esp32BaseConfig::getInt(kNs, kMaxRunMs, 60000));
    config.max_action_pulses = static_cast<uint32_t>(Esp32BaseConfig::getInt(kNs, kMaxActionPulses, 432000));
    return config;
}

uint32_t fa_master_read_next_feed_action_id(void) {
    return readActionId(FaActionIdConfig::KEY_NEXT_FEED, FaActionIdConfig::DEFAULT_NEXT_FEED);
}

uint32_t fa_master_read_next_door_action_id(void) {
    return readActionId(FaActionIdConfig::KEY_NEXT_DOOR, FaActionIdConfig::DEFAULT_NEXT_DOOR);
}

bool fa_master_save_next_feed_action_id(uint32_t next_action_id) {
    return saveActionId(FaActionIdConfig::KEY_NEXT_FEED, next_action_id);
}

bool fa_master_save_next_door_action_id(uint32_t next_action_id) {
    return saveActionId(FaActionIdConfig::KEY_NEXT_DOOR, next_action_id);
}

FaDoorDeviceConfig fa_master_read_door_config(void) {
    FaDoorDeviceConfig config;
    config.station_address = static_cast<uint8_t>(Esp32BaseConfig::getInt(kDoorNs, kDoorStationAddress, 2));
    config.config_version = 1u;
    config.pulses_per_turn = static_cast<uint32_t>(Esp32BaseConfig::getInt(kDoorNs, kDoorPulsesPerTurn, 4320));
    config.travel_pulses = static_cast<uint32_t>(Esp32BaseConfig::getInt(kDoorNs, kDoorTravelPulses, 20000));
    config.open_direction = static_cast<int8_t>(Esp32BaseConfig::getInt(kDoorNs, kDoorOpenDirection, 1));
    config.close_direction = static_cast<int8_t>(Esp32BaseConfig::getInt(kDoorNs, kDoorCloseDirection, -1));
    config.speed_permille = static_cast<uint16_t>(Esp32BaseConfig::getInt(kDoorNs, kDoorSpeedPermille, 700));
    config.accel_ms = 0u;
    config.decel_ms = 0u;
    config.over_current_ma = static_cast<uint16_t>(Esp32BaseConfig::getInt(kDoorNs, kDoorOverCurrentMa, 2500));
    config.over_current_hold_ms = 100u;
    config.stall_detect_ms = 500u;
    config.stall_min_delta_pulses = 10u;
    config.max_run_ms = static_cast<uint32_t>(Esp32BaseConfig::getInt(kDoorNs, kDoorMaxRunMs, 30000));
    config.max_action_pulses = static_cast<uint32_t>(Esp32BaseConfig::getInt(kDoorNs, kDoorMaxActionPulses, 100000));
    return config;
}
