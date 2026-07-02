#include "fa_master_web_internal.h"

#include <string.h>

namespace {

void sendI32(int32_t value) {
    char buf[18];
    snprintf(buf, sizeof(buf), "%ld", static_cast<long>(value));
    Esp32BaseWeb::sendChunk(buf);
}

void sendBoolJson(bool value) {
    Esp32BaseWeb::sendChunk(value ? "true" : "false");
}

const char* recordStateCode(uint8_t state) {
    switch (state) {
    case FA_ACTION_RECORD_RUNNING:
        return "running";
    case FA_ACTION_RECORD_COMPLETED:
        return "completed";
    case FA_ACTION_RECORD_STOPPED:
        return "stopped";
    case FA_ACTION_RECORD_FAILED:
        return "failed";
    default:
        return "unknown";
    }
}

const char* stopReasonCode(uint8_t reason) {
    switch (reason) {
    case FA_STOP_NONE:
        return "none";
    case FA_STOP_TARGET_REACHED:
        return "target_reached";
    case FA_STOP_MASTER_COMMAND:
        return "master_command";
    case FA_STOP_OVER_CURRENT:
        return "over_current";
    case FA_STOP_STALL:
        return "stall";
    case FA_STOP_TIMEOUT:
        return "timeout";
    case FA_STOP_TARGET_OVERRUN:
        return "target_overrun";
    case FA_STOP_WATCHDOG:
        return "watchdog";
    case FA_STOP_LOCAL_FAULT:
        return "local_fault";
    default:
        return "unknown";
    }
}

const char* faultCodeName(uint16_t fault) {
    switch (fault) {
    case FA_FAULT_NONE:
        return "none";
    case FA_FAULT_OVER_CURRENT:
        return "over_current";
    case FA_FAULT_STALL:
        return "stall";
    case FA_FAULT_ENCODER_LOST:
        return "encoder_lost";
    case FA_FAULT_RUN_TIMEOUT:
        return "run_timeout";
    case FA_FAULT_TARGET_OVERRUN:
        return "target_overrun";
    case FA_FAULT_CONFIG_INVALID:
        return "config_invalid";
    case FA_FAULT_DRIVER_ABNORMAL:
        return "driver_abnormal";
    case FA_FAULT_CURRENT_SENSOR:
        return "current_sensor";
    case FA_FAULT_WATCHDOG_RESET:
        return "watchdog_reset";
    case FA_FAULT_RESERVED_ADDRESS:
        return "reserved_address";
    case FA_FAULT_COMMAND_REJECTED:
        return "command_rejected";
    case FA_FAULT_COMMUNICATION:
        return "communication";
    default:
        return "unknown";
    }
}

const char* sourceTypeName(uint8_t source) {
    switch (source) {
    case FA_ACTION_RECORD_SOURCE_MANUAL:
        return "manual";
    case FA_ACTION_RECORD_SOURCE_SCHEDULE:
        return "schedule";
    case FA_ACTION_RECORD_SOURCE_MAINTENANCE:
        return "maintenance";
    default:
        return "unknown";
    }
}

const char* amountModeName(uint8_t mode) {
    switch (mode) {
    case FA_ACTION_RECORD_AMOUNT_TURNS_X1000:
        return "turns_x1000";
    case FA_ACTION_RECORD_AMOUNT_MG:
        return "mg";
    case FA_ACTION_RECORD_AMOUNT_PULSES:
        return "pulses";
    default:
        return "unknown";
    }
}

const char* deviceTypeName(uint8_t type) {
    switch (type) {
    case FA_DEVICE_TYPE_FEEDER:
        return "feeder";
    case FA_DEVICE_TYPE_DOOR:
        return "door";
    default:
        return "unknown";
    }
}

void sendStringProp(const char* key, const char* value) {
    Esp32BaseWeb::sendChunk("\"");
    Esp32BaseWeb::sendChunk(key);
    Esp32BaseWeb::sendChunk("\":\"");
    Esp32BaseWeb::writeJsonEscaped(value != nullptr ? value : "");
    Esp32BaseWeb::sendChunk("\"");
}

void sendRecordJson(const FaActionRecord& record) {
    Esp32BaseWeb::sendChunk("{\"actionId\":");
    sendNumber(record.action_id);
    Esp32BaseWeb::sendChunk(",\"deviceId\":");
    sendNumber(record.device_id);
    Esp32BaseWeb::sendChunk(",");
    sendStringProp("deviceName", record.device_name);
    Esp32BaseWeb::sendChunk(",\"deviceType\":\"");
    Esp32BaseWeb::sendChunk(deviceTypeName(record.device_type));
    Esp32BaseWeb::sendChunk("\",\"actionType\":");
    sendNumber(record.action_type);
    Esp32BaseWeb::sendChunk(",\"sourceType\":\"");
    Esp32BaseWeb::sendChunk(sourceTypeName(record.source_type));
    Esp32BaseWeb::sendChunk("\",\"sourceId\":");
    sendNumber(record.source_id);
    Esp32BaseWeb::sendChunk(",\"busAddress\":");
    sendNumber(record.bus_address);
    Esp32BaseWeb::sendChunk(",\"state\":\"");
    Esp32BaseWeb::sendChunk(recordStateCode(record.state));
    Esp32BaseWeb::sendChunk("\",\"stateText\":\"");
    Esp32BaseWeb::writeJsonEscaped(uiRecordState(record.state));
    Esp32BaseWeb::sendChunk("\",\"amountMode\":\"");
    Esp32BaseWeb::sendChunk(amountModeName(record.amount_mode));
    Esp32BaseWeb::sendChunk("\",\"amountValue\":");
    sendNumber(record.amount_value);
    Esp32BaseWeb::sendChunk(",\"targetPulses\":");
    sendNumber(record.target_pulses);
    Esp32BaseWeb::sendChunk(",\"completedPulses\":");
    sendNumber(record.completed_pulses);
    Esp32BaseWeb::sendChunk(",\"finalPositionPulses\":");
    sendI32(record.final_position_pulses);
    Esp32BaseWeb::sendChunk(",\"runMs\":");
    sendNumber(record.run_ms);
    Esp32BaseWeb::sendChunk(",\"currentMa\":");
    sendNumber(record.current_ma);
    Esp32BaseWeb::sendChunk(",\"peakCurrentMa\":");
    sendNumber(record.peak_current_ma);
    Esp32BaseWeb::sendChunk(",\"startedAt\":");
    sendNumber(record.started_at_s);
    Esp32BaseWeb::sendChunk(",\"endedAt\":");
    sendNumber(record.ended_at_s);
    Esp32BaseWeb::sendChunk(",\"stopReason\":\"");
    Esp32BaseWeb::sendChunk(stopReasonCode(record.stop_reason));
    Esp32BaseWeb::sendChunk("\",\"stopReasonText\":\"");
    Esp32BaseWeb::writeJsonEscaped(uiStopReason(record.stop_reason));
    Esp32BaseWeb::sendChunk("\",\"faultCode\":");
    sendNumber(record.fault_code);
    Esp32BaseWeb::sendChunk(",\"fault\":\"");
    Esp32BaseWeb::sendChunk(faultCodeName(record.fault_code));
    Esp32BaseWeb::sendChunk("\",\"faultText\":\"");
    Esp32BaseWeb::writeJsonEscaped(uiFaultName(record.fault_code));
    Esp32BaseWeb::sendChunk("\"}");
}

void sendDeviceStatusJson(const char* key, const FaWebDeviceStatus& status) {
    Esp32BaseWeb::sendChunk("\"");
    Esp32BaseWeb::sendChunk(key);
    Esp32BaseWeb::sendChunk("\":{\"deviceId\":");
    sendNumber(status.device_id);
    Esp32BaseWeb::sendChunk(",");
    sendStringProp("name", status.device_name);
    Esp32BaseWeb::sendChunk(",\"stationAddress\":");
    sendNumber(status.station_address);
    Esp32BaseWeb::sendChunk(",\"registryReady\":");
    sendBoolJson(status.registry_ready);
    Esp32BaseWeb::sendChunk(",\"hasDevice\":");
    sendBoolJson(status.has_device);
    Esp32BaseWeb::sendChunk(",\"enabled\":");
    sendBoolJson(status.device_enabled);
    Esp32BaseWeb::sendChunk(",\"hasStation\":");
    sendBoolJson(status.has_station);
    Esp32BaseWeb::sendChunk(",\"stationState\":\"");
    Esp32BaseWeb::sendChunk(stationOnlineStateName(status.station_online_state));
    Esp32BaseWeb::sendChunk("\",\"stationStateText\":\"");
    Esp32BaseWeb::writeJsonEscaped(uiStationOnlineState(status.station_online_state));
    Esp32BaseWeb::sendChunk("\",\"lastSeenAt\":");
    sendNumber(status.last_seen_at);
    Esp32BaseWeb::sendChunk(",\"lastError\":");
    sendNumber(status.last_error);
    Esp32BaseWeb::sendChunk("}");
}

void sendEnvJson(const FaEnvSensorSnapshot& env) {
    Esp32BaseWeb::sendChunk("\"env\":{\"enabled\":");
    sendBoolJson(env.enabled);
    Esp32BaseWeb::sendChunk(",\"ready\":");
    sendBoolJson(env.ready);
    Esp32BaseWeb::sendChunk(",\"valid\":");
    sendBoolJson(env.valid);
    Esp32BaseWeb::sendChunk(",\"address\":");
    sendNumber(env.address);
    Esp32BaseWeb::sendChunk(",\"temperatureCX100\":");
    sendI32(env.temperature_c_x100);
    Esp32BaseWeb::sendChunk(",\"humidityX100\":");
    sendNumber(env.humidity_x100);
    Esp32BaseWeb::sendChunk(",\"sampledAt\":");
    sendNumber(env.sampled_at_s);
    Esp32BaseWeb::sendChunk(",\"sampledUptimeMs\":");
    sendNumber(env.sampled_uptime_ms);
    Esp32BaseWeb::sendChunk(",\"successCount\":");
    sendNumber(env.success_count);
    Esp32BaseWeb::sendChunk(",\"failCount\":");
    sendNumber(env.fail_count);
    Esp32BaseWeb::sendChunk(",");
    sendStringProp("lastError", env.last_error);
    Esp32BaseWeb::sendChunk("}");
}

void sendScheduleJson(const FaAutoScheduleState& state) {
    Esp32BaseWeb::sendChunk("\"auto\":{\"timeSynced\":");
    sendBoolJson(state.time_synced);
    Esp32BaseWeb::sendChunk(",\"enabled\":");
    sendBoolJson(state.enabled);
    Esp32BaseWeb::sendChunk(",\"feedEnabled\":");
    sendBoolJson(state.feed_enabled);
    Esp32BaseWeb::sendChunk(",\"doorEnabled\":");
    sendBoolJson(state.door_enabled);
    Esp32BaseWeb::sendChunk(",\"epochSec\":");
    sendNumber(state.epoch_sec);
    Esp32BaseWeb::sendChunk(",\"localDay\":");
    sendI32(state.local_day);
    Esp32BaseWeb::sendChunk(",\"localMinute\":");
    sendNumber(state.local_minute);
    Esp32BaseWeb::sendChunk(",\"feed1Minute\":");
    sendNumber(state.feed_1_minute);
    Esp32BaseWeb::sendChunk(",\"feed1AmountMg\":");
    sendNumber(state.feed_1_amount_mg);
    Esp32BaseWeb::sendChunk(",\"feed2Minute\":");
    sendNumber(state.feed_2_minute);
    Esp32BaseWeb::sendChunk(",\"feed2AmountMg\":");
    sendNumber(state.feed_2_amount_mg);
    Esp32BaseWeb::sendChunk(",\"doorOpenMinute\":");
    sendNumber(state.door_open_minute);
    Esp32BaseWeb::sendChunk(",\"doorCloseMinute\":");
    sendNumber(state.door_close_minute);
    Esp32BaseWeb::sendChunk(",\"feedPauseUntil\":");
    sendNumber(state.feed_pause_until);
    Esp32BaseWeb::sendChunk(",\"doorPauseUntil\":");
    sendNumber(state.door_pause_until);
    Esp32BaseWeb::sendChunk("}");
}

}  // namespace

void sendStatusSummaryApi(void) {
    if (!Esp32BaseWeb::checkAuth()) {
        return;
    }

    FaFeedDeviceConfig feed_config = fa_master_read_feed_config();
    FaDoorDeviceConfig door_config = fa_master_read_door_config();
    FaWebDeviceStatus feed_status;
    FaWebDeviceStatus door_status;
    (void)readDeviceStatus(FA_DEVICE_TYPE_FEEDER, kSingleFeederDeviceId, feed_config.station_address, feed_status);
    (void)readDeviceStatus(FA_DEVICE_TYPE_DOOR, kSingleDoorDeviceId, door_config.station_address, door_status);

    const bool action_busy = g_action_runtime != nullptr && g_action_runtime->isBusy();
    FaEnvSensorSnapshot env = g_env_sensor != nullptr ? g_env_sensor->snapshot() : FaEnvSensorSnapshot();
    FaAutoScheduleState auto_state = g_auto_scheduler != nullptr ? g_auto_scheduler->snapshot() : FaAutoScheduleState();

    Esp32BaseWeb::beginJson(200);
    Esp32BaseWeb::sendChunk("\"ok\":true,\"transport\":{\"ready\":");
    sendBoolJson(g_transport != nullptr && g_transport->isReady());
    Esp32BaseWeb::sendChunk(",\"mode\":\"");
    Esp32BaseWeb::sendChunk(g_transport != nullptr ? FaRs485Transport::modeName(g_transport->config().mode) : "unavailable");
    Esp32BaseWeb::sendChunk("\",\"modeText\":\"");
    Esp32BaseWeb::writeJsonEscaped(g_transport != nullptr ? uiTransportMode(g_transport->config().mode) : "不可用");
    Esp32BaseWeb::sendChunk("\",\"simulated\":");
    sendBoolJson(g_transport != nullptr && g_transport->isSimulated());
    Esp32BaseWeb::sendChunk("},\"action\":{\"busy\":");
    sendBoolJson(action_busy);
    Esp32BaseWeb::sendChunk(",\"lastError\":\"");
    Esp32BaseWeb::writeJsonEscaped(g_action_runtime != nullptr ? g_action_runtime->lastError() : "unavailable");
    Esp32BaseWeb::sendChunk("\",\"lastErrorText\":\"");
    Esp32BaseWeb::writeJsonEscaped(g_action_runtime != nullptr ? uiRuntimeError(g_action_runtime->lastError()) : "不可用");
    Esp32BaseWeb::sendChunk("\"");
    if (action_busy && g_action_runtime->activeRecord() != nullptr) {
        Esp32BaseWeb::sendChunk(",\"active\":");
        sendRecordJson(*g_action_runtime->activeRecord());
    } else {
        Esp32BaseWeb::sendChunk(",\"active\":null");
    }
    Esp32BaseWeb::sendChunk("},\"records\":{\"ready\":");
    sendBoolJson(FaActionRecordStore::isReady());
    Esp32BaseWeb::sendChunk(",\"count\":");
    sendNumber(FaActionRecordStore::count());
    Esp32BaseWeb::sendChunk(",\"capacity\":");
    sendNumber(FaActionRecordStore::capacity());
    Esp32BaseWeb::sendChunk(",\"sequence\":");
    sendNumber(FaActionRecordStore::sequence());
    Esp32BaseWeb::sendChunk("},");
    sendDeviceStatusJson("feeder", feed_status);
    Esp32BaseWeb::sendChunk(",");
    sendDeviceStatusJson("door", door_status);
    Esp32BaseWeb::sendChunk(",");
    sendEnvJson(env);
    Esp32BaseWeb::sendChunk(",");
    sendScheduleJson(auto_state);
    Esp32BaseWeb::endJson();
}

void sendRecentRecordsApi(void) {
    if (!Esp32BaseWeb::checkAuth()) {
        return;
    }

    uint32_t requested_limit = readUIntParam("limit", kRecentRecordLimit);
    if (requested_limit == 0u) {
        requested_limit = kRecentRecordLimit;
    }
    if (requested_limit > 32u) {
        requested_limit = 32u;
    }

    const uint16_t count = FaActionRecordStore::count();
    const uint16_t limit = count < requested_limit ? count : static_cast<uint16_t>(requested_limit);
    Esp32BaseWeb::beginJson(200);
    Esp32BaseWeb::sendChunk("\"ok\":true,\"ready\":");
    sendBoolJson(FaActionRecordStore::isReady());
    Esp32BaseWeb::sendChunk(",\"count\":");
    sendNumber(count);
    Esp32BaseWeb::sendChunk(",\"capacity\":");
    sendNumber(FaActionRecordStore::capacity());
    Esp32BaseWeb::sendChunk(",\"sequence\":");
    sendNumber(FaActionRecordStore::sequence());
    Esp32BaseWeb::sendChunk(",\"limit\":");
    sendNumber(limit);
    Esp32BaseWeb::sendChunk(",\"records\":[");
    uint16_t emitted = 0u;
    for (uint16_t i = 0u; i < limit; ++i) {
        FaActionRecord record;
        if (!FaActionRecordStore::readLatest(i, record)) {
            continue;
        }
        if (emitted != 0u) {
            Esp32BaseWeb::sendChunk(",");
        }
        sendRecordJson(record);
        ++emitted;
    }
    Esp32BaseWeb::sendChunk("]");
    Esp32BaseWeb::endJson();
}
