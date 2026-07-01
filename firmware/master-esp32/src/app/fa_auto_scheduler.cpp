#include "fa_auto_scheduler.h"

#include <Arduino.h>
#include <Esp32Base.h>
#include <string.h>

#include "fa_master_config.h"

namespace {

uint16_t clampMinute(int32_t value, uint16_t fallback) {
    if (value < 0 || value > 1439) {
        return fallback;
    }
    return static_cast<uint16_t>(value);
}

void copyDeviceName(char* out, size_t len, const char* name) {
    if (out == nullptr || len == 0u) {
        return;
    }
    memset(out, 0, len);
    if (name != nullptr) {
        strncpy(out, name, len - 1u);
    }
}

const char* doorCommandName(uint8_t command) {
    return command == FA_DOOR_COMMAND_CLOSE ? "close" : "open";
}

uint8_t parseCommonResponse(const uint8_t* response,
                            size_t response_len,
                            uint8_t address,
                            uint8_t seq,
                            uint8_t cmd,
                            FaMasterCommonResponse& common) {
    const uint8_t status = fa_rs485_master_parse_common(response, response_len, address, seq, cmd, &common);
    if (status != FA_STATUS_OK) {
        return status;
    }
    return common.status_code;
}

}  // namespace

void FaAutoScheduler::begin(FaRs485Master* master,
                            FaRs485Transport* transport,
                            FaFeedService* feed_service,
                            FaDoorService* door_service,
                            FaDeviceRegistry* registry,
                            FaMasterActionRuntime* action_runtime) {
    master_ = master;
    transport_ = transport;
    feed_service_ = feed_service;
    door_service_ = door_service;
    registry_ = registry;
    action_runtime_ = action_runtime;
    last_check_ms_ = 0u;
    last_day_ = -1;
    last_minute_ = -1;
}

void FaAutoScheduler::handle() {
    const uint32_t now_ms = millis();
    if (last_check_ms_ != 0u && now_ms - last_check_ms_ < kCheckIntervalMs) {
        return;
    }
    last_check_ms_ = now_ms;

    FaAutoScheduleState state;
    if (!readSchedule(state) || !state.time_synced || !state.enabled) {
        return;
    }
    if (last_day_ == state.local_day && last_minute_ == static_cast<int16_t>(state.local_minute)) {
        return;
    }
    last_day_ = state.local_day;
    last_minute_ = static_cast<int16_t>(state.local_minute);

    if (state.door_pause_until != 0u && state.door_pause_until <= state.epoch_sec) {
        if (Esp32BaseConfig::setInt(FaAutoScheduleConfig::NS, FaAutoScheduleConfig::KEY_DOOR_PAUSE_UNTIL, 0)) {
            ESP32BASE_LOG_I("farm", "auto_door_resume epoch=%lu", static_cast<unsigned long>(state.epoch_sec));
            state.door_pause_until = 0u;
        }
    }
    if (state.feed_pause_until != 0u && state.feed_pause_until <= state.epoch_sec) {
        if (Esp32BaseConfig::setInt(FaAutoScheduleConfig::NS, FaAutoScheduleConfig::KEY_FEED_PAUSE_UNTIL, 0)) {
            ESP32BASE_LOG_I("farm", "auto_feed_resume epoch=%lu", static_cast<unsigned long>(state.epoch_sec));
            state.feed_pause_until = 0u;
        }
    }

    const bool feed_paused = state.feed_pause_until > state.epoch_sec;
    const bool door_paused = state.door_pause_until > state.epoch_sec;
    if (state.door_enabled && !door_paused && state.local_minute == state.door_open_minute) {
        (void)triggerDoor(1u, FA_DOOR_COMMAND_OPEN);
    }
    if (state.feed_enabled && !feed_paused && state.local_minute == state.feed_1_minute) {
        (void)triggerFeed(1u, state.feed_1_amount_mg);
    }
    if (state.feed_enabled && !feed_paused && state.local_minute == state.feed_2_minute) {
        (void)triggerFeed(2u, state.feed_2_amount_mg);
    }
    if (state.door_enabled && !door_paused && state.local_minute == state.door_close_minute) {
        (void)triggerDoor(2u, FA_DOOR_COMMAND_CLOSE);
    }
}

FaAutoScheduleState FaAutoScheduler::snapshot() const {
    FaAutoScheduleState state;
    (void)readSchedule(state);
    return state;
}

bool FaAutoScheduler::readSchedule(FaAutoScheduleState& state) const {
    state = {};
    const Esp32BaseTime::Snapshot now = Esp32BaseTime::snapshot();
    state.time_synced = now.synced;
    state.epoch_sec = now.epochSec;
    state.enabled = Esp32BaseConfig::getBool(FaAutoScheduleConfig::NS, FaAutoScheduleConfig::KEY_ENABLED, true);
    state.feed_enabled = Esp32BaseConfig::getBool(FaAutoScheduleConfig::NS, FaAutoScheduleConfig::KEY_FEED_ENABLED, true);
    state.door_enabled = Esp32BaseConfig::getBool(FaAutoScheduleConfig::NS, FaAutoScheduleConfig::KEY_DOOR_ENABLED, true);
    state.feed_1_minute = clampMinute(Esp32BaseConfig::getInt(FaAutoScheduleConfig::NS, FaAutoScheduleConfig::KEY_FEED_1_MIN, 430), 430u);
    state.feed_1_amount_mg = static_cast<uint32_t>(Esp32BaseConfig::getInt(FaAutoScheduleConfig::NS, FaAutoScheduleConfig::KEY_FEED_1_AMOUNT_MG, 100000));
    state.feed_2_minute = clampMinute(Esp32BaseConfig::getInt(FaAutoScheduleConfig::NS, FaAutoScheduleConfig::KEY_FEED_2_MIN, 1090), 1090u);
    state.feed_2_amount_mg = static_cast<uint32_t>(Esp32BaseConfig::getInt(FaAutoScheduleConfig::NS, FaAutoScheduleConfig::KEY_FEED_2_AMOUNT_MG, 100000));
    state.door_open_minute = clampMinute(Esp32BaseConfig::getInt(FaAutoScheduleConfig::NS, FaAutoScheduleConfig::KEY_DOOR_OPEN_MIN, 480), 480u);
    state.door_close_minute = clampMinute(Esp32BaseConfig::getInt(FaAutoScheduleConfig::NS, FaAutoScheduleConfig::KEY_DOOR_CLOSE_MIN, 1050), 1050u);
    state.feed_pause_until = static_cast<uint32_t>(Esp32BaseConfig::getInt(FaAutoScheduleConfig::NS, FaAutoScheduleConfig::KEY_FEED_PAUSE_UNTIL, 0));
    state.door_pause_until = static_cast<uint32_t>(Esp32BaseConfig::getInt(FaAutoScheduleConfig::NS, FaAutoScheduleConfig::KEY_DOOR_PAUSE_UNTIL, 0));

    const int32_t tz_offset_sec = Esp32BaseConfig::getInt(FaAutoScheduleConfig::NS, FaAutoScheduleConfig::KEY_TZ_OFFSET_MIN, 480) * 60;
    const int64_t local_sec = static_cast<int64_t>(now.epochSec) + static_cast<int64_t>(tz_offset_sec);
    if (local_sec < 0) {
        state.local_day = -1;
        state.local_minute = 0u;
        return true;
    }
    state.local_day = static_cast<int32_t>(local_sec / 86400);
    state.local_minute = static_cast<uint16_t>((local_sec % 86400) / 60);
    return true;
}

bool FaAutoScheduler::triggerFeed(uint8_t source_id, uint32_t amount_mg) {
    if (master_ == nullptr || transport_ == nullptr || feed_service_ == nullptr || action_runtime_ == nullptr) {
        return false;
    }
    if (action_runtime_->isBusy()) {
        ESP32BASE_LOG_W("farm", "auto_feed_skipped reason=action_busy source=%u", source_id);
        return false;
    }
    if (!transport_->isReady()) {
        ESP32BASE_LOG_W("farm", "auto_feed_skipped reason=transport_not_configured source=%u", source_id);
        return false;
    }

    uint16_t device_id = kSingleFeederDeviceId;
    char device_name[FA_ACTION_RECORD_DEVICE_NAME_LEN] = {};
    FaFeedDeviceConfig config = fa_master_read_feed_config();
    if (!prepareDevice(FA_DEVICE_TYPE_FEEDER, kSingleFeederDeviceId, config.station_address, device_id, device_name, sizeof(device_name))) {
        return false;
    }

    FaMasterMotorConfig motor_config;
    uint8_t status = fa_feed_make_motor_config(&config, &motor_config);
    if (status != FA_STATUS_OK || !sendMotorConfig(config.station_address, motor_config, "auto_feed_set_motor_config")) {
        ESP32BASE_LOG_W("farm", "auto_feed_skipped reason=motor_config status=%u source=%u", status, source_id);
        return false;
    }

    FaMasterActionRequest action;
    FaFeedResult result;
    status = fa_feed_make_manual_action(feed_service_, &config, FA_FEED_AMOUNT_MG, amount_mg, &action, &result);
    if (status != FA_STATUS_OK) {
        ESP32BASE_LOG_W("farm", "auto_feed_skipped reason=make_action status=%u source=%u", status, source_id);
        return false;
    }
    if (!sendAction(config.station_address, action, "auto_feed_start_action")) {
        return false;
    }

    FaActionRecordStart start = {};
    start.action_id = action.action_id;
    start.device_id = device_id;
    start.bus_address = config.station_address;
    start.device_type = action.device_type;
    start.action_type = action.action_type;
    start.source_type = FA_ACTION_RECORD_SOURCE_SCHEDULE;
    start.source_id = source_id;
    start.target_pulses = result.target_pulses;
    start.amount_mode = FA_FEED_AMOUNT_MG;
    start.amount_value = amount_mg;
    start.started_at_s = FaMasterActionRuntime::nowSeconds();
    copyDeviceName(start.device_name, sizeof(start.device_name), device_name);
    const bool tracking = action_runtime_->trackStartedAction(start);
    ESP32BASE_LOG_I("farm", "auto_feed_sent source=%u action_id=%lu device_id=%u addr=%u amount_mg=%lu target=%lu tracking=%s",
                    source_id,
                    static_cast<unsigned long>(action.action_id),
                    device_id,
                    config.station_address,
                    static_cast<unsigned long>(amount_mg),
                    static_cast<unsigned long>(result.target_pulses),
                    tracking ? "running" : action_runtime_->lastError());
    return tracking;
}

bool FaAutoScheduler::triggerDoor(uint8_t source_id, uint8_t command) {
    if (master_ == nullptr || transport_ == nullptr || door_service_ == nullptr || action_runtime_ == nullptr) {
        return false;
    }
    if (action_runtime_->isBusy()) {
        ESP32BASE_LOG_W("farm", "auto_door_skipped reason=action_busy source=%u command=%s",
                        source_id,
                        doorCommandName(command));
        return false;
    }
    if (!transport_->isReady()) {
        ESP32BASE_LOG_W("farm", "auto_door_skipped reason=transport_not_configured source=%u command=%s",
                        source_id,
                        doorCommandName(command));
        return false;
    }

    uint16_t device_id = kSingleDoorDeviceId;
    char device_name[FA_ACTION_RECORD_DEVICE_NAME_LEN] = {};
    FaDoorDeviceConfig config = fa_master_read_door_config();
    if (!prepareDevice(FA_DEVICE_TYPE_DOOR, kSingleDoorDeviceId, config.station_address, device_id, device_name, sizeof(device_name))) {
        return false;
    }

    FaMasterMotorConfig motor_config;
    uint8_t status = fa_door_make_motor_config(&config, &motor_config);
    if (status != FA_STATUS_OK || !sendMotorConfig(config.station_address, motor_config, "auto_door_set_motor_config")) {
        ESP32BASE_LOG_W("farm", "auto_door_skipped reason=motor_config status=%u source=%u command=%s",
                        status,
                        source_id,
                        doorCommandName(command));
        return false;
    }

    FaMasterActionRequest action;
    FaDoorResult result;
    status = fa_door_make_action(door_service_, &config, command, &action, &result);
    if (status != FA_STATUS_OK) {
        ESP32BASE_LOG_W("farm", "auto_door_skipped reason=make_action status=%u source=%u command=%s",
                        status,
                        source_id,
                        doorCommandName(command));
        return false;
    }
    if (!sendAction(config.station_address, action, "auto_door_start_action")) {
        return false;
    }

    FaActionRecordStart start = {};
    start.action_id = action.action_id;
    start.device_id = device_id;
    start.bus_address = config.station_address;
    start.device_type = action.device_type;
    start.action_type = action.action_type;
    start.source_type = FA_ACTION_RECORD_SOURCE_SCHEDULE;
    start.source_id = source_id;
    start.target_pulses = result.target_pulses;
    start.amount_mode = FA_ACTION_RECORD_AMOUNT_PULSES;
    start.amount_value = result.target_pulses;
    start.started_at_s = FaMasterActionRuntime::nowSeconds();
    copyDeviceName(start.device_name, sizeof(start.device_name), device_name);
    const bool tracking = action_runtime_->trackStartedAction(start);
    ESP32BASE_LOG_I("farm", "auto_door_sent source=%u command=%s action_id=%lu device_id=%u addr=%u target=%lu tracking=%s",
                    source_id,
                    doorCommandName(command),
                    static_cast<unsigned long>(action.action_id),
                    device_id,
                    config.station_address,
                    static_cast<unsigned long>(result.target_pulses),
                    tracking ? "running" : action_runtime_->lastError());
    return tracking;
}

bool FaAutoScheduler::prepareDevice(uint8_t device_type,
                                    uint16_t fallback_device_id,
                                    uint8_t& station_address,
                                    uint16_t& device_id,
                                    char* device_name,
                                    size_t device_name_len) {
    device_id = fallback_device_id;
    if (registry_ == nullptr || !registry_->isReady()) {
        ESP32BASE_LOG_W("farm", "auto_action_skipped reason=registry_unavailable type=%u", device_type);
        return false;
    }

    FaDeviceRecord device;
    if (!registry_->deviceByType(device_type, device)) {
        ESP32BASE_LOG_W("farm", "auto_action_skipped reason=device_missing type=%u", device_type);
        return false;
    }
    device_id = device.device_id;
    copyDeviceName(device_name, device_name_len, device.name);
    if (device.enabled == 0u) {
        ESP32BASE_LOG_W("farm", "auto_action_skipped reason=device_disabled device_id=%u type=%u",
                        device.device_id,
                        device_type);
        return false;
    }

    FaStationRecord station;
    if (registry_->stationById(device.station_id, station)) {
        if (!fa_address_is_normal(station.bus_address)) {
            ESP32BASE_LOG_W("farm", "auto_action_skipped reason=bad_station_address device_id=%u station_id=%u",
                            device.device_id,
                            device.station_id);
            return false;
        }
        if (station.enabled == 0u) {
            ESP32BASE_LOG_W("farm", "auto_action_skipped reason=station_disabled device_id=%u addr=%u",
                            device.device_id,
                            station.bus_address);
            return false;
        }
        if (station.online_state != FA_STATION_ONLINE_UNKNOWN &&
            station.online_state != FA_STATION_ONLINE_ONLINE) {
            ESP32BASE_LOG_W("farm", "auto_action_skipped reason=station_not_ready device_id=%u addr=%u state=%u error=%u",
                            device.device_id,
                            station.bus_address,
                            station.online_state,
                            station.last_error);
            return false;
        }
        station_address = station.bus_address;
    }
    return true;
}

bool FaAutoScheduler::sendMotorConfig(uint8_t station_address, const FaMasterMotorConfig& motor_config, const char* stage) {
    uint8_t request[FA_MAX_FRAME_LEN];
    uint8_t response[FA_MAX_FRAME_LEN];
    size_t request_len = 0u;
    size_t response_len = 0u;
    uint8_t seq = 0u;
    const FaFrameResult frame_result = fa_rs485_master_build_set_motor_config(master_,
                                                                             station_address,
                                                                             &motor_config,
                                                                             request,
                                                                             sizeof(request),
                                                                             &request_len,
                                                                             &seq);
    if (frame_result != FA_FRAME_OK) {
        ESP32BASE_LOG_W("farm", "%s_failed addr=%u error=frame_%u", stage, station_address, static_cast<uint16_t>(frame_result));
        return false;
    }
    const FaRs485TransportStatus tx_status = transport_->transact(request, request_len, response, sizeof(response), &response_len, 0u);
    if (tx_status != FaRs485TransportStatus::OK) {
        ESP32BASE_LOG_W("farm", "%s_failed addr=%u error=%s", stage, station_address, FaRs485Transport::statusName(tx_status));
        return false;
    }
    FaMasterCommonResponse common;
    const uint8_t status = parseCommonResponse(response, response_len, station_address, seq, FA_CMD_SET_MOTOR_CONFIG, common);
    if (status != FA_STATUS_OK) {
        ESP32BASE_LOG_W("farm", "%s_rejected addr=%u status=%u fault=%u", stage, station_address, status, common.fault_code);
        return false;
    }
    return true;
}

bool FaAutoScheduler::sendAction(uint8_t station_address, const FaMasterActionRequest& action, const char* stage) {
    uint8_t request[FA_MAX_FRAME_LEN];
    uint8_t response[FA_MAX_FRAME_LEN];
    size_t request_len = 0u;
    size_t response_len = 0u;
    uint8_t seq = 0u;
    const FaFrameResult frame_result = fa_rs485_master_build_start_action(master_,
                                                                          station_address,
                                                                          &action,
                                                                          request,
                                                                          sizeof(request),
                                                                          &request_len,
                                                                          &seq);
    if (frame_result != FA_FRAME_OK) {
        ESP32BASE_LOG_W("farm", "%s_failed addr=%u action_id=%lu error=frame_%u",
                        stage,
                        station_address,
                        static_cast<unsigned long>(action.action_id),
                        static_cast<uint16_t>(frame_result));
        return false;
    }
    const FaRs485TransportStatus tx_status = transport_->transact(request, request_len, response, sizeof(response), &response_len, 0u);
    if (tx_status != FaRs485TransportStatus::OK) {
        ESP32BASE_LOG_W("farm", "%s_failed addr=%u action_id=%lu error=%s",
                        stage,
                        station_address,
                        static_cast<unsigned long>(action.action_id),
                        FaRs485Transport::statusName(tx_status));
        return false;
    }
    FaMasterCommonResponse common;
    const uint8_t status = parseCommonResponse(response, response_len, station_address, seq, FA_CMD_START_ACTION, common);
    if (status != FA_STATUS_OK) {
        ESP32BASE_LOG_W("farm", "%s_rejected addr=%u action_id=%lu status=%u fault=%u",
                        stage,
                        station_address,
                        static_cast<unsigned long>(action.action_id),
                        status,
                        common.fault_code);
        return false;
    }
    return true;
}
