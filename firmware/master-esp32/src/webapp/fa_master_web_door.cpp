#include "fa_master_web_internal.h"

namespace {

const char* doorCommandName(uint8_t command) {
    return command == FA_DOOR_COMMAND_CLOSE ? "close" : "open";
}

void sendManualDoorActionApi(uint8_t command) {
    if (!Esp32BaseWeb::checkPostAllowed("door_manual")) {
        return;
    }
    if (g_door_service == nullptr || g_rs485_master == nullptr || g_transport == nullptr || g_action_runtime == nullptr) {
        Esp32BaseWeb::sendJson(503, "{\"ok\":false,\"error\":\"service_unavailable\"}");
        return;
    }
    if (g_action_runtime->isBusy()) {
        ESP32BASE_LOG_W("farm", "door_manual_blocked action_busy command=%s", doorCommandName(command));
        Esp32BaseWeb::sendJson(409, "{\"ok\":false,\"error\":\"action_busy\"}");
        return;
    }

    FaDoorDeviceConfig config = fa_master_read_door_config();
    FaWebDeviceStatus deviceStatus;
    (void)readDeviceStatus(FA_DEVICE_TYPE_DOOR, kSingleDoorDeviceId, config.station_address, deviceStatus);
    config.station_address = deviceStatus.station_address;
    if (!deviceStatus.device_enabled) {
        ESP32BASE_LOG_W("farm", "door_manual_blocked device_disabled command=%s device_id=%u addr=%u",
                        doorCommandName(command),
                        deviceStatus.device_id,
                        config.station_address);
        Esp32BaseWeb::sendJson(409, "{\"ok\":false,\"error\":\"device_disabled\"}");
        return;
    }
    FaMasterActionRequest action;
    FaDoorResult result;
    FaDoorService preview_service = *g_door_service;
    uint8_t status = fa_door_make_action(&preview_service, &config, command, &action, &result);
    if (status != FA_STATUS_OK) {
        Esp32BaseWeb::beginJson(400);
        Esp32BaseWeb::sendChunk("\"ok\":false,\"status\":\"");
        Esp32BaseWeb::sendChunk(statusName(status));
        Esp32BaseWeb::sendChunk("\"");
        Esp32BaseWeb::endJson();
        return;
    }

    if (!g_transport->isReady()) {
        ESP32BASE_LOG_I("farm", "door_manual_preview command=%s action_id=%lu device_id=%u addr=%u target=%lu",
                        doorCommandName(command),
                        static_cast<unsigned long>(action.action_id),
                        deviceStatus.device_id,
                        config.station_address,
                        static_cast<unsigned long>(result.target_pulses));
        Esp32BaseWeb::beginJson(200);
        Esp32BaseWeb::sendChunk("\"ok\":true,\"dryRun\":true,\"transport\":\"not_configured\",\"command\":\"");
        Esp32BaseWeb::sendChunk(doorCommandName(command));
        Esp32BaseWeb::sendChunk("\",\"actionId\":");
        sendNumber(action.action_id);
        Esp32BaseWeb::sendChunk(",\"stationAddress\":");
        sendNumber(config.station_address);
        Esp32BaseWeb::sendChunk(",\"targetPulses\":");
        sendNumber(result.target_pulses);
        Esp32BaseWeb::sendChunk(",\"message\":\"RS485 pins are not configured; action was built but not sent\"");
        Esp32BaseWeb::endJson();
        return;
    }

    if (deviceStatusBlocksStart(deviceStatus)) {
        sendDeviceStatusBlockedJson(deviceStatus);
        return;
    }

    FaMasterMotorConfig motor_config;
    status = fa_door_make_motor_config(&config, &motor_config);
    if (status != FA_STATUS_OK) {
        Esp32BaseWeb::beginJson(400);
        Esp32BaseWeb::sendChunk("\"ok\":false,\"status\":\"");
        Esp32BaseWeb::sendChunk(statusName(status));
        Esp32BaseWeb::sendChunk("\"");
        Esp32BaseWeb::endJson();
        return;
    }

    uint8_t request[FA_MAX_FRAME_LEN];
    size_t request_len = 0u;
    uint8_t seq = 0u;
    FaFrameResult frame_result = fa_rs485_master_build_set_motor_config(g_rs485_master,
                                                                        config.station_address,
                                                                        &motor_config,
                                                                        request,
                                                                        sizeof(request),
                                                                        &request_len,
                                                                        &seq);
    if (frame_result != FA_FRAME_OK) {
        sendFeedTransportError(500, "door_set_motor_config", "frame", frameResultName(frame_result));
        return;
    }

    FaMasterCommonResponse common;
    if (!transactAndParseCommon(config.station_address, seq, FA_CMD_SET_MOTOR_CONFIG, request, request_len, &common, "door_set_motor_config")) {
        return;
    }

    status = fa_door_make_action(g_door_service, &config, command, &action, &result);
    if (status != FA_STATUS_OK) {
        Esp32BaseWeb::beginJson(400);
        Esp32BaseWeb::sendChunk("\"ok\":false,\"status\":\"");
        Esp32BaseWeb::sendChunk(statusName(status));
        Esp32BaseWeb::sendChunk("\"");
        Esp32BaseWeb::endJson();
        return;
    }

    request_len = 0u;
    seq = 0u;
    frame_result = fa_rs485_master_build_start_action(g_rs485_master,
                                                      config.station_address,
                                                      &action,
                                                      request,
                                                      sizeof(request),
                                                      &request_len,
                                                      &seq);
    if (frame_result != FA_FRAME_OK) {
        sendFeedTransportError(500, "door_start_action", "frame", frameResultName(frame_result));
        return;
    }
    if (!transactAndParseCommon(config.station_address, seq, FA_CMD_START_ACTION, request, request_len, &common, "door_start_action")) {
        return;
    }

    FaActionRecordStart record_start = {};
    record_start.action_id = action.action_id;
    record_start.device_id = deviceStatus.device_id;
    record_start.bus_address = config.station_address;
    record_start.device_type = action.device_type;
    record_start.action_type = action.action_type;
    record_start.source_type = FA_ACTION_RECORD_SOURCE_MANUAL;
    record_start.source_id = 0u;
    record_start.target_pulses = result.target_pulses;
    record_start.amount_mode = FA_ACTION_RECORD_AMOUNT_PULSES;
    record_start.amount_value = result.target_pulses;
    record_start.started_at_s = FaMasterActionRuntime::nowSeconds();
    copyActionRecordDeviceName(record_start, deviceStatus);
    const bool tracking = g_action_runtime->trackStartedAction(record_start);

    ESP32BASE_LOG_I("farm", "door_manual_sent command=%s action_id=%lu device_id=%u addr=%u target=%lu tracking=%s",
                    doorCommandName(command),
                    static_cast<unsigned long>(action.action_id),
                    deviceStatus.device_id,
                    config.station_address,
                    static_cast<unsigned long>(result.target_pulses),
                    tracking ? "running" : g_action_runtime->lastError());
    Esp32BaseWeb::beginJson(200);
    Esp32BaseWeb::sendChunk("\"ok\":true,\"dryRun\":false,\"transport\":\"ready\",\"command\":\"");
    Esp32BaseWeb::sendChunk(doorCommandName(command));
    Esp32BaseWeb::sendChunk("\",\"actionId\":");
    sendNumber(action.action_id);
    Esp32BaseWeb::sendChunk(",\"stationAddress\":");
    sendNumber(config.station_address);
    Esp32BaseWeb::sendChunk(",\"targetPulses\":");
    sendNumber(result.target_pulses);
    Esp32BaseWeb::sendChunk(",\"tracking\":\"");
    Esp32BaseWeb::sendChunk(tracking ? "running" : g_action_runtime->lastError());
    Esp32BaseWeb::sendChunk("\",\"message\":\"door action accepted by station\"");
    Esp32BaseWeb::endJson();
}

}  // namespace

void sendDoorStopApi(void) {
    if (!Esp32BaseWeb::checkPostAllowed("door_stop")) {
        return;
    }
    if (g_rs485_master == nullptr || g_transport == nullptr) {
        Esp32BaseWeb::sendJson(503, "{\"ok\":false,\"error\":\"service_unavailable\"}");
        return;
    }
    FaDoorDeviceConfig config = fa_master_read_door_config();
    FaWebDeviceStatus deviceStatus;
    (void)readDeviceStatus(FA_DEVICE_TYPE_DOOR, kSingleDoorDeviceId, config.station_address, deviceStatus);
    config.station_address = deviceStatus.station_address;
    if (!g_transport->isReady()) {
        Esp32BaseWeb::sendJson(503, "{\"ok\":false,\"transport\":\"not_configured\"}");
        return;
    }

    uint8_t request[FA_MAX_FRAME_LEN];
    size_t request_len = 0u;
    uint8_t seq = 0u;
    const FaFrameResult frame_result = fa_rs485_master_build_stop_action(g_rs485_master,
                                                                        config.station_address,
                                                                        request,
                                                                        sizeof(request),
                                                                        &request_len,
                                                                        &seq);
    if (frame_result != FA_FRAME_OK) {
        sendFeedTransportError(500, "door_stop", "frame", frameResultName(frame_result));
        return;
    }
    FaMasterCommonResponse common;
    if (!transactAndParseCommon(config.station_address, seq, FA_CMD_STOP_ACTION, request, request_len, &common, "door_stop")) {
        return;
    }

    ESP32BASE_LOG_I("farm", "door_stop_accepted addr=%u", config.station_address);
    Esp32BaseWeb::sendJson(200, "{\"ok\":true,\"command\":\"stop\",\"message\":\"stop accepted by station\"}");
}

void sendDoorOpenApi(void) {
    sendManualDoorActionApi(FA_DOOR_COMMAND_OPEN);
}

void sendDoorCloseApi(void) {
    sendManualDoorActionApi(FA_DOOR_COMMAND_CLOSE);
}

void sendDoorPage(void) {
    if (!Esp32BaseWeb::checkAuth()) {
        return;
    }

    FaDoorDeviceConfig config = fa_master_read_door_config();
    FaWebDeviceStatus deviceStatus;
    (void)readDeviceStatus(FA_DEVICE_TYPE_DOOR, kSingleDoorDeviceId, config.station_address, deviceStatus);
    config.station_address = deviceStatus.station_address;
    char deviceLabel[36];
    char stationStatus[36];
    formatDeviceLabel(deviceStatus.device_id, deviceLabel, sizeof(deviceLabel));
    formatStationStatusLabel(deviceStatus, stationStatus, sizeof(stationStatus));
    char value[24];

    Esp32BaseWeb::sendHeader("Door");
    Esp32BaseWeb::sendPageTitle("Manual door", "Sends one bounded motor action for open or close, and can stop the active station action.");

    Esp32BaseWeb::beginMetricGrid();
    snprintf(value, sizeof(value), "%u", config.station_address);
    Esp32BaseWeb::sendMetric("Station", value, "RS485 address");
    snprintf(value, sizeof(value), "%lu", static_cast<unsigned long>(config.travel_pulses));
    Esp32BaseWeb::sendMetric("Travel", value, "pulses");
    snprintf(value, sizeof(value), "%d / %d", config.open_direction, config.close_direction);
    Esp32BaseWeb::sendMetric("Direction", value, "open / close");
    Esp32BaseWeb::sendMetric("RS485", g_transport != nullptr && g_transport->isReady() ? "ready" : "not configured");
    Esp32BaseWeb::sendMetric("Device", deviceLabel, deviceStatus.device_enabled ? "enabled" : "disabled");
    Esp32BaseWeb::sendMetric("Station state", stationStatus);
    Esp32BaseWeb::sendMetric("Action", g_action_runtime != nullptr && g_action_runtime->isBusy() ? "running" : "idle");
    Esp32BaseWeb::endMetricGrid();

    Esp32BaseWeb::beginPanel("Run");
    Esp32BaseWeb::sendChunk("<div class='actions'>");
    Esp32BaseWeb::sendChunk("<form method='post' action='/api/door/open' onsubmit='return once(this)'><input type='submit' value='Open'></form>");
    Esp32BaseWeb::sendChunk("<form method='post' action='/api/door/close' onsubmit='return once(this)'><input type='submit' value='Close'></form>");
    Esp32BaseWeb::sendChunk("<form method='post' action='/api/door/stop' onsubmit='return once(this)'><input class='danger' type='submit' value='Stop'></form>");
    Esp32BaseWeb::sendChunk("</div>");
    Esp32BaseWeb::endPanel();

    sendActiveActionPanel();
    sendRecentRecordsPanel();

    Esp32BaseWeb::sendInfoRowCompactLink("Door parameters", "Station address, travel pulses, directions and safety limits are stored by Esp32Base App Config.", "App Config", "/esp32base/app-config", "Edit", Esp32BaseWeb::UI_INFO);
    Esp32BaseWeb::sendFooter();
}
