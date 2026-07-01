#include "fa_master_web_internal.h"

#include <stdlib.h>

namespace {

bool readIntParamStrict(const char* name, int32_t& out) {
    char raw[18] = "";
    if (!Esp32BaseWeb::getParam(name, raw, sizeof(raw)) || raw[0] == '\0') {
        return false;
    }
    char* end = nullptr;
    const long value = strtol(raw, &end, 10);
    if (end == raw || *end != '\0') {
        return false;
    }
    out = static_cast<int32_t>(value);
    return true;
}

bool readBoundedInt(const char* name, int32_t min_value, int32_t max_value, int32_t& out) {
    if (!readIntParamStrict(name, out)) {
        return false;
    }
    return out >= min_value && out <= max_value;
}

bool setIntChecked(const char* ns, const char* key, int32_t value) {
    return Esp32BaseConfig::setInt(ns, key, value);
}

void sendBadParam(const char* name) {
    Esp32BaseWeb::beginJson(400);
    Esp32BaseWeb::sendChunk("\"ok\":false,\"error\":\"bad_param\",\"field\":\"");
    Esp32BaseWeb::sendChunk(name);
    Esp32BaseWeb::sendChunk("\"");
    Esp32BaseWeb::endJson();
}

void sendSaveFailed(const char* scope) {
    Esp32BaseWeb::beginJson(500);
    Esp32BaseWeb::sendChunk("\"ok\":false,\"error\":\"save_failed\",\"scope\":\"");
    Esp32BaseWeb::sendChunk(scope);
    Esp32BaseWeb::sendChunk("\"");
    Esp32BaseWeb::endJson();
}

void sendSaved(const char* scope) {
    Esp32BaseWeb::beginJson(200);
    Esp32BaseWeb::sendChunk("\"ok\":true,\"scope\":\"");
    Esp32BaseWeb::sendChunk(scope);
    Esp32BaseWeb::sendChunk("\",\"message\":\"saved\"");
    Esp32BaseWeb::endJson();
}

}  // namespace

void sendAutoScheduleSaveApi(void) {
    if (!Esp32BaseWeb::checkPostAllowed("auto_schedule_save")) {
        return;
    }

    int32_t enabled = 0;
    int32_t feed_enabled = 0;
    int32_t door_enabled = 0;
    int32_t feed_1_min = 0;
    int32_t feed_1_amount = 0;
    int32_t feed_2_min = 0;
    int32_t feed_2_amount = 0;
    int32_t door_open_min = 0;
    int32_t door_close_min = 0;
    if (!readBoundedInt("enabled", 0, 1, enabled)) {
        sendBadParam("enabled");
        return;
    }
    if (!readBoundedInt("feedEnabled", 0, 1, feed_enabled)) {
        sendBadParam("feedEnabled");
        return;
    }
    if (!readBoundedInt("doorEnabled", 0, 1, door_enabled)) {
        sendBadParam("doorEnabled");
        return;
    }
    if (!readBoundedInt("feed1Min", 0, 1439, feed_1_min)) {
        sendBadParam("feed1Min");
        return;
    }
    if (!readBoundedInt("feed1AmountMg", 1, 5000000, feed_1_amount)) {
        sendBadParam("feed1AmountMg");
        return;
    }
    if (!readBoundedInt("feed2Min", 0, 1439, feed_2_min)) {
        sendBadParam("feed2Min");
        return;
    }
    if (!readBoundedInt("feed2AmountMg", 1, 5000000, feed_2_amount)) {
        sendBadParam("feed2AmountMg");
        return;
    }
    if (!readBoundedInt("doorOpenMin", 0, 1439, door_open_min)) {
        sendBadParam("doorOpenMin");
        return;
    }
    if (!readBoundedInt("doorCloseMin", 0, 1439, door_close_min)) {
        sendBadParam("doorCloseMin");
        return;
    }

    const bool ok =
        setIntChecked(FaAutoScheduleConfig::NS, FaAutoScheduleConfig::KEY_ENABLED, enabled) &&
        setIntChecked(FaAutoScheduleConfig::NS, FaAutoScheduleConfig::KEY_FEED_ENABLED, feed_enabled) &&
        setIntChecked(FaAutoScheduleConfig::NS, FaAutoScheduleConfig::KEY_DOOR_ENABLED, door_enabled) &&
        setIntChecked(FaAutoScheduleConfig::NS, FaAutoScheduleConfig::KEY_FEED_1_MIN, feed_1_min) &&
        setIntChecked(FaAutoScheduleConfig::NS, FaAutoScheduleConfig::KEY_FEED_1_AMOUNT_MG, feed_1_amount) &&
        setIntChecked(FaAutoScheduleConfig::NS, FaAutoScheduleConfig::KEY_FEED_2_MIN, feed_2_min) &&
        setIntChecked(FaAutoScheduleConfig::NS, FaAutoScheduleConfig::KEY_FEED_2_AMOUNT_MG, feed_2_amount) &&
        setIntChecked(FaAutoScheduleConfig::NS, FaAutoScheduleConfig::KEY_DOOR_OPEN_MIN, door_open_min) &&
        setIntChecked(FaAutoScheduleConfig::NS, FaAutoScheduleConfig::KEY_DOOR_CLOSE_MIN, door_close_min);
    if (!ok) {
        sendSaveFailed("auto");
        return;
    }

    ESP32BASE_LOG_I("farm", "auto_schedule_saved feed1=%ld/%ld feed2=%ld/%ld door=%ld/%ld enabled=%ld/%ld/%ld",
                    static_cast<long>(feed_1_min),
                    static_cast<long>(feed_1_amount),
                    static_cast<long>(feed_2_min),
                    static_cast<long>(feed_2_amount),
                    static_cast<long>(door_open_min),
                    static_cast<long>(door_close_min),
                    static_cast<long>(enabled),
                    static_cast<long>(feed_enabled),
                    static_cast<long>(door_enabled));
    sendSaved("auto");
}

void sendFeedConfigSaveApi(void) {
    if (!Esp32BaseWeb::checkPostAllowed("feed_config_save")) {
        return;
    }

    int32_t station = 0;
    int32_t pulses = 0;
    int32_t grams = 0;
    int32_t direction = 0;
    int32_t speed = 0;
    int32_t over_current = 0;
    int32_t max_run = 0;
    int32_t max_pulses = 0;
    if (!readBoundedInt("stationAddress", 1, 127, station)) {
        sendBadParam("stationAddress");
        return;
    }
    if (!readBoundedInt("pulsesPerTurn", 1, 200000, pulses)) {
        sendBadParam("pulsesPerTurn");
        return;
    }
    if (!readBoundedInt("gramsPerTurnMg", 1, 1000000, grams)) {
        sendBadParam("gramsPerTurnMg");
        return;
    }
    if (!readBoundedInt("direction", -1, 1, direction) || direction == 0) {
        sendBadParam("direction");
        return;
    }
    if (!readBoundedInt("speedPermille", 1, 1000, speed)) {
        sendBadParam("speedPermille");
        return;
    }
    if (!readBoundedInt("overCurrentMa", 1, 10000, over_current)) {
        sendBadParam("overCurrentMa");
        return;
    }
    if (!readBoundedInt("maxRunMs", 100, 600000, max_run)) {
        sendBadParam("maxRunMs");
        return;
    }
    if (!readBoundedInt("maxActionPulses", 1, 2000000, max_pulses)) {
        sendBadParam("maxActionPulses");
        return;
    }

    const bool ok =
        setIntChecked(kNs, kStationAddress, station) &&
        setIntChecked(kNs, kPulsesPerTurn, pulses) &&
        setIntChecked(kNs, kGramsPerTurnMg, grams) &&
        setIntChecked(kNs, kDirection, direction) &&
        setIntChecked(kNs, kSpeedPermille, speed) &&
        setIntChecked(kNs, kOverCurrentMa, over_current) &&
        setIntChecked(kNs, kMaxRunMs, max_run) &&
        setIntChecked(kNs, kMaxActionPulses, max_pulses);
    if (!ok) {
        sendSaveFailed("feed");
        return;
    }

    ESP32BASE_LOG_I("farm", "feed_config_saved addr=%ld ppt=%ld gpt_mg=%ld dir=%ld speed=%ld oc=%ld max_ms=%ld max_p=%ld",
                    static_cast<long>(station),
                    static_cast<long>(pulses),
                    static_cast<long>(grams),
                    static_cast<long>(direction),
                    static_cast<long>(speed),
                    static_cast<long>(over_current),
                    static_cast<long>(max_run),
                    static_cast<long>(max_pulses));
    sendSaved("feed");
}

void sendDoorConfigSaveApi(void) {
    if (!Esp32BaseWeb::checkPostAllowed("door_config_save")) {
        return;
    }

    int32_t station = 0;
    int32_t pulses = 0;
    int32_t travel = 0;
    int32_t open_dir = 0;
    int32_t close_dir = 0;
    int32_t speed = 0;
    int32_t over_current = 0;
    int32_t max_run = 0;
    int32_t max_pulses = 0;
    if (!readBoundedInt("stationAddress", 1, 127, station)) {
        sendBadParam("stationAddress");
        return;
    }
    if (!readBoundedInt("pulsesPerTurn", 1, 200000, pulses)) {
        sendBadParam("pulsesPerTurn");
        return;
    }
    if (!readBoundedInt("travelPulses", 1, 2000000, travel)) {
        sendBadParam("travelPulses");
        return;
    }
    if (!readBoundedInt("openDirection", -1, 1, open_dir) || open_dir == 0) {
        sendBadParam("openDirection");
        return;
    }
    if (!readBoundedInt("closeDirection", -1, 1, close_dir) || close_dir == 0) {
        sendBadParam("closeDirection");
        return;
    }
    if (!readBoundedInt("speedPermille", 1, 1000, speed)) {
        sendBadParam("speedPermille");
        return;
    }
    if (!readBoundedInt("overCurrentMa", 1, 10000, over_current)) {
        sendBadParam("overCurrentMa");
        return;
    }
    if (!readBoundedInt("maxRunMs", 100, 600000, max_run)) {
        sendBadParam("maxRunMs");
        return;
    }
    if (!readBoundedInt("maxActionPulses", 1, 2000000, max_pulses)) {
        sendBadParam("maxActionPulses");
        return;
    }

    const bool ok =
        setIntChecked(kDoorNs, kDoorStationAddress, station) &&
        setIntChecked(kDoorNs, kDoorPulsesPerTurn, pulses) &&
        setIntChecked(kDoorNs, kDoorTravelPulses, travel) &&
        setIntChecked(kDoorNs, kDoorOpenDirection, open_dir) &&
        setIntChecked(kDoorNs, kDoorCloseDirection, close_dir) &&
        setIntChecked(kDoorNs, kDoorSpeedPermille, speed) &&
        setIntChecked(kDoorNs, kDoorOverCurrentMa, over_current) &&
        setIntChecked(kDoorNs, kDoorMaxRunMs, max_run) &&
        setIntChecked(kDoorNs, kDoorMaxActionPulses, max_pulses);
    if (!ok) {
        sendSaveFailed("door");
        return;
    }

    ESP32BASE_LOG_I("farm", "door_config_saved addr=%ld ppt=%ld travel=%ld open_dir=%ld close_dir=%ld speed=%ld oc=%ld max_ms=%ld max_p=%ld",
                    static_cast<long>(station),
                    static_cast<long>(pulses),
                    static_cast<long>(travel),
                    static_cast<long>(open_dir),
                    static_cast<long>(close_dir),
                    static_cast<long>(speed),
                    static_cast<long>(over_current),
                    static_cast<long>(max_run),
                    static_cast<long>(max_pulses));
    sendSaved("door");
}
