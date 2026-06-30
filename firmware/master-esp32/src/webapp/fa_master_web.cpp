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
    case FA_STATUS_ERR_SAFETY_BLOCKED:
        return "safety_blocked";
    default:
        return "error";
    }
}

void sendNumber(uint32_t value) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%lu", static_cast<unsigned long>(value));
    Esp32BaseWeb::sendChunk(buf);
}

void sendFeedPage(void) {
    if (!Esp32BaseWeb::checkAuth()) {
        return;
    }

    const FaFeedDeviceConfig config = readFeedConfig();
    char value[24];

    Esp32BaseWeb::sendHeader("Feed");
    Esp32BaseWeb::sendPageTitle("Manual feed", "Builds one bounded feeder action from saved device parameters.");

    Esp32BaseWeb::beginMetricGrid();
    snprintf(value, sizeof(value), "%u", config.station_address);
    Esp32BaseWeb::sendMetric("Station", value, "RS485 address");
    snprintf(value, sizeof(value), "%lu", static_cast<unsigned long>(config.pulses_per_turn));
    Esp32BaseWeb::sendMetric("Pulses/turn", value);
    snprintf(value, sizeof(value), "%lu", static_cast<unsigned long>(config.grams_per_turn_mg));
    Esp32BaseWeb::sendMetric("mg/turn", value);
    snprintf(value, sizeof(value), "%u", FaActionRecordStore::count());
    Esp32BaseWeb::sendMetric("Records", value, FaActionRecordStore::isReady() ? "LittleFS ring ready" : "Store unavailable");
    Esp32BaseWeb::endMetricGrid();

    Esp32BaseWeb::beginPanel("Run");
    Esp32BaseWeb::sendChunk("<form method='post' action='/api/feed/manual' onsubmit='return once(this)'><div class='fieldgrid'>");
    Esp32BaseWeb::sendChunk("<div class='field med'><label>Amount</label><input type='number' name='amount' min='1' max='1000000' value='4000'><small>Stored as mg or turns x1000.</small></div>");
    Esp32BaseWeb::sendChunk("<div class='field med'><label>Mode</label><select name='mode'><option value='mg'>mg</option><option value='turns'>turns x1000</option></select><small>Use App Config for calibration.</small></div>");
    Esp32BaseWeb::sendChunk("</div><div class='actions'><input type='submit' value='Build action'></div></form>");
    Esp32BaseWeb::endPanel();

    Esp32BaseWeb::sendInfoRowCompactLink("Feeder parameters", "Station address, pulses/turn, grams/turn and safety limits are stored by Esp32Base App Config.", "App Config", "/esp32base/app-config", "Edit", Esp32BaseWeb::UI_INFO);

    Esp32BaseWeb::sendFooter();
}

void sendManualFeedApi(void) {
    if (!Esp32BaseWeb::checkPostAllowed("feed_manual")) {
        return;
    }
    if (g_feed_service == nullptr) {
        Esp32BaseWeb::sendJson(503, "{\"ok\":false,\"error\":\"feed_service_unavailable\"}");
        return;
    }

    const FaFeedDeviceConfig config = readFeedConfig();
    char modeText[12] = "";
    (void)Esp32BaseWeb::getParam("mode", modeText, sizeof(modeText));
    const uint8_t amountMode = strcmp(modeText, "turns") == 0 ? FA_FEED_AMOUNT_TURNS_X1000 : FA_FEED_AMOUNT_MG;
    const uint32_t amount = readUIntParam("amount", 0u);

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
    Esp32BaseWeb::sendChunk("\"ok\":true,\"dryRun\":true,\"transport\":\"not_connected\",\"actionId\":");
    sendNumber(action.action_id);
    Esp32BaseWeb::sendChunk(",\"stationAddress\":");
    sendNumber(config.station_address);
    Esp32BaseWeb::sendChunk(",\"targetPulses\":");
    sendNumber(result.target_pulses);
    Esp32BaseWeb::sendChunk(",\"speedPermille\":");
    sendNumber(action.speed_permille);
    Esp32BaseWeb::sendChunk(",\"message\":\"RS485 transport is not wired yet; action was built but not sent\"");
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
}

void fa_master_web_register_routes(FaFeedService *feed_service) {
    g_feed_service = feed_service;
    Esp32BaseWeb::addPage("/feed", "Feed", sendFeedPage);
    Esp32BaseWeb::addApi("/api/feed/manual", sendManualFeedApi);
}
