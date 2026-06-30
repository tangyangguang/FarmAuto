#include "fa_master_web.h"

#include <Arduino.h>
#include <Esp32Base.h>
#include <stdlib.h>
#include <string.h>

#include "fa_action_record_store.h"

namespace {

constexpr const char* kNs = "fa_feed";
constexpr const char* kStationAddress = "addr";
constexpr const char* kPulsesPerTurn = "ppt";
constexpr const char* kGramsPerTurnMg = "gpt_mg";
constexpr const char* kDirection = "dir";
constexpr const char* kSpeedPermille = "speed";
constexpr const char* kOverCurrentMa = "oc_ma";
constexpr const char* kMaxRunMs = "max_ms";
constexpr const char* kMaxActionPulses = "max_p";

FaFeedService* g_feed_service = nullptr;
FaRs485Master* g_rs485_master = nullptr;
FaRs485Transport* g_transport = nullptr;

uint32_t readUIntParam(const char* name, uint32_t fallback) {
    char raw[16] = "";
    if (!Esp32BaseWeb::getParam(name, raw, sizeof(raw)) || raw[0] == '\0') {
        return fallback;
    }
    char* end = nullptr;
    const unsigned long value = strtoul(raw, &end, 10);
    if (end == raw || *end != '\0') {
        return fallback;
    }
    return static_cast<uint32_t>(value);
}

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

const char* statusName(uint8_t status) {
    switch (status) {
    case FA_STATUS_OK:
        return "ok";
    case FA_STATUS_ERR_BAD_PARAM:
        return "bad_param";
    case FA_STATUS_ERR_NOT_CONFIGURED:
        return "not_configured";
    case FA_STATUS_ERR_BUSY:
        return "busy";
    case FA_STATUS_ERR_FAULT_ACTIVE:
        return "fault_active";
    case FA_STATUS_ERR_ACTION_DUPLICATE:
        return "action_duplicate";
    case FA_STATUS_ERR_SAFETY_BLOCKED:
        return "safety_blocked";
    case FA_STATUS_ERR_BAD_LENGTH:
        return "bad_length";
    case FA_STATUS_ERR_UNSUPPORTED_VERSION:
        return "unsupported_version";
    case FA_STATUS_ERR_ADDRESS_RESERVED:
        return "address_reserved";
    case FA_STATUS_ERR_INTERNAL:
        return "internal";
    default:
        return "error";
    }
}

void sendNumber(uint32_t value) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%lu", static_cast<unsigned long>(value));
    Esp32BaseWeb::sendChunk(buf);
}

const char* frameResultName(FaFrameResult result) {
    switch (result) {
    case FA_FRAME_OK:
        return "ok";
    case FA_FRAME_ERR_NULL:
        return "null";
    case FA_FRAME_ERR_TOO_SHORT:
        return "too_short";
    case FA_FRAME_ERR_BAD_SOF:
        return "bad_sof";
    case FA_FRAME_ERR_UNSUPPORTED_VERSION:
        return "unsupported_version";
    case FA_FRAME_ERR_PAYLOAD_TOO_LONG:
        return "payload_too_long";
    case FA_FRAME_ERR_LENGTH_MISMATCH:
        return "length_mismatch";
    case FA_FRAME_ERR_CRC:
        return "crc";
    case FA_FRAME_ERR_OUTPUT_TOO_SMALL:
        return "output_too_small";
    default:
        return "frame_error";
    }
}

void sendFeedTransportError(uint16_t http_code, const char* stage, const char* error_key, const char* error_value) {
    Esp32BaseWeb::beginJson(http_code);
    Esp32BaseWeb::sendChunk("\"ok\":false,\"stage\":\"");
    Esp32BaseWeb::sendChunk(stage);
    Esp32BaseWeb::sendChunk("\",\"");
    Esp32BaseWeb::sendChunk(error_key);
    Esp32BaseWeb::sendChunk("\":\"");
    Esp32BaseWeb::sendChunk(error_value);
    Esp32BaseWeb::sendChunk("\"");
    Esp32BaseWeb::endJson();
}

bool transactAndParseCommon(uint8_t station_address,
                            uint8_t expected_seq,
                            uint8_t expected_cmd,
                            const uint8_t* request,
                            size_t request_len,
                            FaMasterCommonResponse* common,
                            const char* stage) {
    if (common != nullptr) {
        memset(common, 0, sizeof(*common));
    }
    uint8_t response[FA_MAX_FRAME_LEN];
    size_t response_len = 0u;
    const FaRs485TransportStatus tx_status = g_transport->transact(request, request_len, response, sizeof(response), &response_len, 0u);
    if (tx_status != FaRs485TransportStatus::OK) {
        sendFeedTransportError(tx_status == FaRs485TransportStatus::TIMEOUT ? 504 : 502,
                               stage,
                               "transport",
                               FaRs485Transport::statusName(tx_status));
        return false;
    }

    const uint8_t status = fa_rs485_master_parse_common(response, response_len, station_address, expected_seq, expected_cmd, common);
    if (status != FA_STATUS_OK) {
        Esp32BaseWeb::beginJson(502);
        Esp32BaseWeb::sendChunk("\"ok\":false,\"stage\":\"");
        Esp32BaseWeb::sendChunk(stage);
        Esp32BaseWeb::sendChunk("\",\"status\":\"");
        Esp32BaseWeb::sendChunk(statusName(status));
        Esp32BaseWeb::sendChunk("\",\"faultCode\":");
        sendNumber(common != nullptr ? common->fault_code : 0u);
        Esp32BaseWeb::endJson();
        return false;
    }
    return true;
}

void sendFeedPage(void) {
    if (!Esp32BaseWeb::checkAuth()) {
        return;
    }

    const FaFeedDeviceConfig config = readFeedConfig();
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
    Esp32BaseWeb::endMetricGrid();

    Esp32BaseWeb::beginPanel("Run");
    Esp32BaseWeb::sendChunk("<form method='post' action='/api/feed/manual' onsubmit='return once(this)'><div class='fieldgrid'>");
    Esp32BaseWeb::sendChunk("<div class='field med'><label>Amount</label><input type='number' name='amount' min='1' max='1000000' value='4000'><small>Stored as mg or turns x1000.</small></div>");
    Esp32BaseWeb::sendChunk("<div class='field med'><label>Mode</label><select name='mode'><option value='mg'>mg</option><option value='turns'>turns x1000</option></select><small>Use App Config for calibration.</small></div>");
    Esp32BaseWeb::sendChunk("</div><div class='actions'><input type='submit' value='Run / preview'></div></form>");
    Esp32BaseWeb::endPanel();

    Esp32BaseWeb::sendInfoRowCompactLink("Feeder parameters", "Station address, pulses/turn, grams/turn and safety limits are stored by Esp32Base App Config.", "App Config", "/esp32base/app-config", "Edit", Esp32BaseWeb::UI_INFO);

    Esp32BaseWeb::sendFooter();
}

void sendManualFeedApi(void) {
    if (!Esp32BaseWeb::checkPostAllowed("feed_manual")) {
        return;
    }
    if (g_feed_service == nullptr || g_rs485_master == nullptr || g_transport == nullptr) {
        Esp32BaseWeb::sendJson(503, "{\"ok\":false,\"error\":\"service_unavailable\"}");
        return;
    }

    const FaFeedDeviceConfig config = readFeedConfig();
    char modeText[12] = "";
    (void)Esp32BaseWeb::getParam("mode", modeText, sizeof(modeText));
    const uint8_t amountMode = strcmp(modeText, "turns") == 0 ? FA_FEED_AMOUNT_TURNS_X1000 : FA_FEED_AMOUNT_MG;
    const uint32_t amount = readUIntParam("amount", 0u);

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

    Esp32BaseWeb::beginJson(200);
    Esp32BaseWeb::sendChunk("\"ok\":true,\"dryRun\":false,\"transport\":\"ready\",\"actionId\":");
    sendNumber(action.action_id);
    Esp32BaseWeb::sendChunk(",\"stationAddress\":");
    sendNumber(config.station_address);
    Esp32BaseWeb::sendChunk(",\"targetPulses\":");
    sendNumber(result.target_pulses);
    Esp32BaseWeb::sendChunk(",\"speedPermille\":");
    sendNumber(action.speed_permille);
    Esp32BaseWeb::sendChunk(",\"message\":\"action accepted by station\"");
    Esp32BaseWeb::endJson();
}

}  // namespace

void fa_master_web_register_config(void) {
    Esp32BaseWeb::setDeviceName("FarmAuto");
    Esp32BaseWeb::setHomePath("/feed");
    Esp32BaseWeb::setHomeMode(Esp32BaseWeb::HOME_COMBINED);
    Esp32BaseWeb::setSystemNavMode(Esp32BaseWeb::SYSTEM_NAV_SECTION);
    Esp32BaseAppConfig::setTitle("FarmAuto Config");
    Esp32BaseAppConfig::addGroup({"feeder", "Feeder"});
    Esp32BaseAppConfig::addGroup({"rs485", "RS485"});
    Esp32BaseAppConfig::addInt({"feeder", kNs, kStationAddress, "Station address", 1, 1, 127, 1, nullptr,
                                "RS485 address 1..127.", false, nullptr});
    Esp32BaseAppConfig::addInt({"feeder", kNs, kPulsesPerTurn, "Pulses per turn", 4320, 1, 200000, 1, "pulses",
                                "Output shaft encoder pulses per turn.", false, nullptr});
    Esp32BaseAppConfig::addInt({"feeder", kNs, kGramsPerTurnMg, "mg per turn", 8000, 1, 1000000, 1, "mg",
                                "Calibration for mg based feeding.", false, nullptr});
    Esp32BaseAppConfig::addInt({"feeder", kNs, kDirection, "Direction", 1, -1, 1, 1, nullptr,
                                "1 forward, -1 reverse.", false, nullptr});
    Esp32BaseAppConfig::addInt({"feeder", kNs, kSpeedPermille, "Speed", 800, 1, 1000, 1, "permille",
                                "Motor speed request.", false, nullptr});
    Esp32BaseAppConfig::addInt({"feeder", kNs, kOverCurrentMa, "Over-current", 2000, 1, 10000, 1, "mA",
                                "Station protection threshold.", false, nullptr});
    Esp32BaseAppConfig::addInt({"feeder", kNs, kMaxRunMs, "Max run", 60000, 100, 600000, 100, "ms",
                                "Single action timeout.", false, nullptr});
    Esp32BaseAppConfig::addInt({"feeder", kNs, kMaxActionPulses, "Max pulses", 432000, 1, 2000000, 1, "pulses",
                                "Single action pulse limit.", false, nullptr});
    Esp32BaseAppConfig::addInt({"rs485", FaRs485Config::NS, FaRs485Config::KEY_UART, "UART", 2, 1, 2, 1, nullptr,
                                "ESP32 hardware serial port.", true, nullptr});
    Esp32BaseAppConfig::addInt({"rs485", FaRs485Config::NS, FaRs485Config::KEY_RX_PIN, "RX pin", -1, -1, 39, 1, nullptr,
                                "-1 disables RS485 transport.", true, nullptr});
    Esp32BaseAppConfig::addInt({"rs485", FaRs485Config::NS, FaRs485Config::KEY_TX_PIN, "TX pin", -1, -1, 39, 1, nullptr,
                                "-1 disables RS485 transport; GPIO34-39 are input-only and invalid here.", true, nullptr});
    Esp32BaseAppConfig::addInt({"rs485", FaRs485Config::NS, FaRs485Config::KEY_DE_PIN, "DE pin", -1, -1, 39, 1, nullptr,
                                "Driver enable pin; GPIO34-39 are input-only and invalid here.", true, nullptr});
    Esp32BaseAppConfig::addInt({"rs485", FaRs485Config::NS, FaRs485Config::KEY_BAUD, "Baud", 115200, 9600, 1000000, 1, "bps",
                                "Default bus rate is 115200.", true, nullptr});
    Esp32BaseAppConfig::addInt({"rs485", FaRs485Config::NS, FaRs485Config::KEY_TIMEOUT_MS, "Timeout", 80, 20, 2000, 1, "ms",
                                "Single request timeout.", true, nullptr});
}

void fa_master_web_register_routes(FaFeedService *feed_service, FaRs485Master *rs485_master, FaRs485Transport *transport) {
    g_feed_service = feed_service;
    g_rs485_master = rs485_master;
    g_transport = transport;
    Esp32BaseWeb::addPage("/feed", "Feed", sendFeedPage);
    Esp32BaseWeb::addApi("/api/feed/manual", sendManualFeedApi);
}
