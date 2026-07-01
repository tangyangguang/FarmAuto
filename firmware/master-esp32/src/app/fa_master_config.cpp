#include "fa_master_config.h"

#include <Esp32Base.h>

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
