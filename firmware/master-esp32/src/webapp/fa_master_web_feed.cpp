#include "fa_master_web_internal.h"

#include <string.h>

void sendFeedPage(void) {
    if (!Esp32BaseWeb::checkAuth()) {
        return;
    }

    FaFeedDeviceConfig config = fa_master_read_feed_config();
    FaWebDeviceStatus deviceStatus;
    (void)readDeviceStatus(FA_DEVICE_TYPE_FEEDER, kSingleFeederDeviceId, config.station_address, deviceStatus);
    config.station_address = deviceStatus.station_address;
    char deviceLabel[36];
    char stationStatus[36];
    formatDeviceLabel(deviceStatus.device_id, deviceLabel, sizeof(deviceLabel));
    formatStationStatusLabel(deviceStatus, stationStatus, sizeof(stationStatus));
    char value[24];

    Esp32BaseWeb::sendHeader("下料");
    Esp32BaseWeb::sendPageTitle("手动下料", "通过 RS485 下发一次有界下料动作；未连接真实 485 时只走当前配置的通讯模式。");

    Esp32BaseWeb::beginMetricGrid();
    snprintf(value, sizeof(value), "%u", config.station_address);
    Esp32BaseWeb::sendMetric("分站", value, "RS485 地址");
    snprintf(value, sizeof(value), "%lu", static_cast<unsigned long>(config.pulses_per_turn));
    Esp32BaseWeb::sendMetric("每圈脉冲", value);
    snprintf(value, sizeof(value), "%lu", static_cast<unsigned long>(config.grams_per_turn_mg));
    Esp32BaseWeb::sendMetric("每圈毫克", value);
    snprintf(value, sizeof(value), "%u", FaActionRecordStore::count());
    Esp32BaseWeb::sendMetric("记录", value, FaActionRecordStore::isReady() ? "LittleFS 环形记录就绪" : "记录不可用");
    Esp32BaseWeb::sendMetric("RS485",
                             g_transport != nullptr && g_transport->isReady() ? uiTransportMode(g_transport->config().mode) : "未配置");
    Esp32BaseWeb::sendMetric("设备", deviceLabel, uiEnabled(deviceStatus.device_enabled));
    Esp32BaseWeb::sendMetric("分站状态", stationStatus);
    Esp32BaseWeb::sendMetric("动作", uiActionState(g_action_runtime != nullptr && g_action_runtime->isBusy()));
    Esp32BaseWeb::endMetricGrid();

    Esp32BaseWeb::beginPanel("执行");
    Esp32BaseWeb::sendChunk("<form method='post' action='/api/feed/manual' onsubmit='return once(this)'><div class='fieldgrid'>");
    Esp32BaseWeb::sendChunk("<div class='field med'><label>数量</label><input type='number' name='amount' min='1' max='1000000' value='4000'><small>按毫克或千分之一圈填写。</small></div>");
    Esp32BaseWeb::sendChunk("<div class='field med'><label>模式</label><select name='mode'><option value='mg'>毫克</option><option value='turns'>千分之一圈</option></select><small>标定参数在配置页维护。</small></div>");
    Esp32BaseWeb::sendChunk("</div><div class='actions'><input type='submit' value='执行'></div></form>");
    Esp32BaseWeb::endPanel();

    sendActiveActionPanel();
    sendRecentRecordsPanel();

    Esp32BaseWeb::sendInfoRowCompactLink("下料参数", "分站地址、每圈脉冲、每圈克数和保护阈值保存在配置页。", "配置", "/esp32base/app-config", "修改", Esp32BaseWeb::UI_INFO);

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

    FaFeedDeviceConfig config = fa_master_read_feed_config();
    FaWebDeviceStatus deviceStatus;
    (void)readDeviceStatus(FA_DEVICE_TYPE_FEEDER, kSingleFeederDeviceId, config.station_address, deviceStatus);
    config.station_address = deviceStatus.station_address;
    if (!deviceStatus.device_enabled) {
        ESP32BASE_LOG_W("farm", "feed_manual_blocked device_disabled device_id=%u addr=%u",
                        deviceStatus.device_id,
                        config.station_address);
        Esp32BaseWeb::sendJson(409, "{\"ok\":false,\"error\":\"device_disabled\"}");
        return;
    }
    char modeText[12] = "";
    (void)Esp32BaseWeb::getParam("mode", modeText, sizeof(modeText));
    const uint8_t amountMode = strcmp(modeText, "turns") == 0 ? FA_FEED_AMOUNT_TURNS_X1000 : FA_FEED_AMOUNT_MG;
    const uint32_t amount = readUIntParam("amount", 0u);

    if (g_action_runtime->isBusy()) {
        ESP32BASE_LOG_W("farm", "feed_manual_blocked action_busy addr=%u amount=%lu mode=%u",
                        config.station_address,
                        static_cast<unsigned long>(amount),
                        amountMode);
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

        ESP32BASE_LOG_I("farm", "feed_manual_preview action_id=%lu device_id=%u addr=%u amount=%lu mode=%u target=%lu speed=%u",
                        static_cast<unsigned long>(action.action_id),
                        deviceStatus.device_id,
                        config.station_address,
                        static_cast<unsigned long>(amount),
                        amountMode,
                        static_cast<unsigned long>(result.target_pulses),
                        action.speed_permille);
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

    if (deviceStatusBlocksStart(deviceStatus)) {
        sendDeviceStatusBlockedJson(deviceStatus);
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
    record_start.device_id = deviceStatus.device_id;
    record_start.bus_address = config.station_address;
    record_start.device_type = action.device_type;
    record_start.action_type = action.action_type;
    record_start.source_type = FA_ACTION_RECORD_SOURCE_MANUAL;
    record_start.source_id = 0u;
    record_start.target_pulses = result.target_pulses;
    record_start.amount_mode = amountMode;
    record_start.amount_value = amount;
    record_start.started_at_s = FaMasterActionRuntime::nowSeconds();
    copyActionRecordDeviceName(record_start, deviceStatus);
    const bool tracking = g_action_runtime->trackStartedAction(record_start);

    ESP32BASE_LOG_I("farm", "feed_manual_sent action_id=%lu device_id=%u addr=%u amount=%lu mode=%u target=%lu speed=%u tracking=%s",
                    static_cast<unsigned long>(action.action_id),
                    deviceStatus.device_id,
                    config.station_address,
                    static_cast<unsigned long>(amount),
                    amountMode,
                    static_cast<unsigned long>(result.target_pulses),
                    action.speed_permille,
                    tracking ? "running" : g_action_runtime->lastError());
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
