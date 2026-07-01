#include "fa_master_web_internal.h"

#include <string.h>

namespace {

FaFeedDeviceConfig readFeedConfig(void) {
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

bool applyFeedDeviceRegistry(FaFeedDeviceConfig& config, uint16_t& device_id, bool& disabled) {
    disabled = false;
    device_id = kSingleFeederDeviceId;
    if (g_device_registry == nullptr || !g_device_registry->isReady()) {
        return true;
    }

    FaDeviceRecord device;
    if (!g_device_registry->deviceByType(FA_DEVICE_TYPE_FEEDER, device)) {
        return true;
    }
    device_id = device.device_id;

    FaStationRecord station;
    if (g_device_registry->stationById(device.station_id, station) && fa_address_is_normal(station.bus_address)) {
        config.station_address = station.bus_address;
    }
    if (device.enabled == 0u) {
        disabled = true;
        return false;
    }
    return true;
}

}  // namespace

void sendFeedPage(void) {
    if (!Esp32BaseWeb::checkAuth()) {
        return;
    }

    FaFeedDeviceConfig config = readFeedConfig();
    uint16_t deviceId = kSingleFeederDeviceId;
    bool deviceDisabled = false;
    (void)applyFeedDeviceRegistry(config, deviceId, deviceDisabled);
    char value[24];

    Esp32BaseWeb::sendHeader("Feed");
    Esp32BaseWeb::sendPageTitle("Manual feed", "Sends one bounded feeder action when RS485 is configured; otherwise previews it.");

    Esp32BaseWeb::beginMetricGrid();
    snprintf(value, sizeof(value), "%u", config.station_address);
    Esp32BaseWeb::sendMetric("Station", value, "RS485 address");
    snprintf(value, sizeof(value), "%lu", static_cast<unsigned long>(config.pulses_per_turn));
    Esp32BaseWeb::sendMetric("Pulses/turn", value);
    snprintf(value, sizeof(value), "%lu", static_cast<unsigned long>(config.grams_per_turn_mg));
    Esp32BaseWeb::sendMetric("mg/turn", value);
    snprintf(value, sizeof(value), "%u", FaActionRecordStore::count());
    Esp32BaseWeb::sendMetric("Records", value, FaActionRecordStore::isReady() ? "LittleFS ring ready" : "Store unavailable");
    Esp32BaseWeb::sendMetric("RS485", g_transport != nullptr && g_transport->isReady() ? "ready" : "not configured");
    Esp32BaseWeb::sendMetric("Device", deviceDisabled ? "disabled" : "enabled");
    Esp32BaseWeb::sendMetric("Action", g_action_runtime != nullptr && g_action_runtime->isBusy() ? "running" : "idle");
    Esp32BaseWeb::endMetricGrid();

    Esp32BaseWeb::beginPanel("Run");
    Esp32BaseWeb::sendChunk("<form method='post' action='/api/feed/manual' onsubmit='return once(this)'><div class='fieldgrid'>");
    Esp32BaseWeb::sendChunk("<div class='field med'><label>Amount</label><input type='number' name='amount' min='1' max='1000000' value='4000'><small>Stored as mg or turns x1000.</small></div>");
    Esp32BaseWeb::sendChunk("<div class='field med'><label>Mode</label><select name='mode'><option value='mg'>mg</option><option value='turns'>turns x1000</option></select><small>Use App Config for calibration.</small></div>");
    Esp32BaseWeb::sendChunk("</div><div class='actions'><input type='submit' value='Run / preview'></div></form>");
    Esp32BaseWeb::endPanel();

    sendActiveActionPanel();
    sendRecentRecordsPanel();

    Esp32BaseWeb::sendInfoRowCompactLink("Feeder parameters", "Station address, pulses/turn, grams/turn and safety limits are stored by Esp32Base App Config.", "App Config", "/esp32base/app-config", "Edit", Esp32BaseWeb::UI_INFO);

    Esp32BaseWeb::sendFooter();
}

void sendManualFeedApi(void) {
    if (!Esp32BaseWeb::checkPostAllowed("feed_manual")) {
        return;
    }
    if (g_feed_service == nullptr || g_rs485_master == nullptr || g_transport == nullptr || g_action_runtime == nullptr) {
        Esp32BaseWeb::sendJson(503, "{\"ok\":false,\"error\":\"service_unavailable\"}");
        return;
    }

    FaFeedDeviceConfig config = readFeedConfig();
    uint16_t deviceId = kSingleFeederDeviceId;
    bool deviceDisabled = false;
    (void)applyFeedDeviceRegistry(config, deviceId, deviceDisabled);
    if (deviceDisabled) {
        Esp32BaseWeb::sendJson(409, "{\"ok\":false,\"error\":\"device_disabled\"}");
        return;
    }
    char modeText[12] = "";
    (void)Esp32BaseWeb::getParam("mode", modeText, sizeof(modeText));
    const uint8_t amountMode = strcmp(modeText, "turns") == 0 ? FA_FEED_AMOUNT_TURNS_X1000 : FA_FEED_AMOUNT_MG;
    const uint32_t amount = readUIntParam("amount", 0u);

    if (g_action_runtime->isBusy()) {
        Esp32BaseWeb::sendJson(409, "{\"ok\":false,\"error\":\"action_busy\"}");
        return;
    }

    if (!g_transport->isReady()) {
        FaFeedService preview_service = *g_feed_service;
        FaMasterActionRequest action;
        FaFeedResult result;
        const uint8_t status = fa_feed_make_manual_action(&preview_service, &config, amountMode, amount, &action, &result);
        if (status != FA_STATUS_OK) {
            Esp32BaseWeb::beginJson(400);
            Esp32BaseWeb::sendChunk("\"ok\":false,\"status\":\"");
            Esp32BaseWeb::sendChunk(statusName(status));
            Esp32BaseWeb::sendChunk("\"");
            Esp32BaseWeb::endJson();
            return;
        }

        Esp32BaseWeb::beginJson(200);
        Esp32BaseWeb::sendChunk("\"ok\":true,\"dryRun\":true,\"transport\":\"not_configured\",\"actionId\":");
        sendNumber(action.action_id);
        Esp32BaseWeb::sendChunk(",\"stationAddress\":");
        sendNumber(config.station_address);
        Esp32BaseWeb::sendChunk(",\"targetPulses\":");
        sendNumber(result.target_pulses);
        Esp32BaseWeb::sendChunk(",\"speedPermille\":");
        sendNumber(action.speed_permille);
        Esp32BaseWeb::sendChunk(",\"message\":\"RS485 pins are not configured; action was built but not sent\"");
        Esp32BaseWeb::endJson();
        return;
    }

    FaMasterMotorConfig motor_config;
    uint8_t status = fa_feed_make_motor_config(&config, &motor_config);
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
        sendFeedTransportError(500, "set_motor_config", "frame", frameResultName(frame_result));
        return;
    }

    FaMasterCommonResponse common;
    if (!transactAndParseCommon(config.station_address, seq, FA_CMD_SET_MOTOR_CONFIG, request, request_len, &common, "set_motor_config")) {
        return;
    }

    FaMasterActionRequest action;
    FaFeedResult result;
    status = fa_feed_make_manual_action(g_feed_service, &config, amountMode, amount, &action, &result);
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
        sendFeedTransportError(500, "start_action", "frame", frameResultName(frame_result));
        return;
    }
    if (!transactAndParseCommon(config.station_address, seq, FA_CMD_START_ACTION, request, request_len, &common, "start_action")) {
        return;
    }

    FaActionRecordStart record_start = {};
    record_start.action_id = action.action_id;
    record_start.device_id = deviceId;
    record_start.bus_address = config.station_address;
    record_start.device_type = action.device_type;
    record_start.action_type = action.action_type;
    record_start.source_type = FA_ACTION_RECORD_SOURCE_MANUAL;
    record_start.source_id = 0u;
    record_start.target_pulses = result.target_pulses;
    record_start.amount_mode = amountMode;
    record_start.amount_value = amount;
    record_start.started_at_s = FaMasterActionRuntime::nowSeconds();
    const bool tracking = g_action_runtime->trackStartedAction(record_start);

    Esp32BaseWeb::beginJson(200);
    Esp32BaseWeb::sendChunk("\"ok\":true,\"dryRun\":false,\"transport\":\"ready\",\"actionId\":");
    sendNumber(action.action_id);
    Esp32BaseWeb::sendChunk(",\"stationAddress\":");
    sendNumber(config.station_address);
    Esp32BaseWeb::sendChunk(",\"targetPulses\":");
    sendNumber(result.target_pulses);
    Esp32BaseWeb::sendChunk(",\"speedPermille\":");
    sendNumber(action.speed_permille);
    Esp32BaseWeb::sendChunk(",\"tracking\":\"");
    Esp32BaseWeb::sendChunk(tracking ? "running" : g_action_runtime->lastError());
    Esp32BaseWeb::sendChunk("\"");
    Esp32BaseWeb::sendChunk(",\"message\":\"action accepted by station\"");
    Esp32BaseWeb::endJson();
}
