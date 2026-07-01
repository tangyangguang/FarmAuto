#include "fa_master_web_internal.h"

namespace {

void formatFixed2Signed(int32_t value_x100, char* out, size_t len) {
    const bool negative = value_x100 < 0;
    const uint32_t abs_value = static_cast<uint32_t>(negative ? -value_x100 : value_x100);
    snprintf(out, len, "%s%lu.%02lu",
             negative ? "-" : "",
             static_cast<unsigned long>(abs_value / 100u),
             static_cast<unsigned long>(abs_value % 100u));
}

void formatFixed2(uint32_t value_x100, char* out, size_t len) {
    snprintf(out, len, "%lu.%02lu",
             static_cast<unsigned long>(value_x100 / 100u),
             static_cast<unsigned long>(value_x100 % 100u));
}

void sendSignedNumber(int32_t value) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%ld", static_cast<long>(value));
    Esp32BaseWeb::sendChunk(buf);
}

}  // namespace

void sendEnvPage(void) {
    if (!Esp32BaseWeb::checkAuth()) {
        return;
    }

    const FaEnvSensorSnapshot state = g_env_sensor != nullptr ? g_env_sensor->snapshot() : FaEnvSensorSnapshot();
    char temp[18];
    char humidity[18];
    char value[32];
    formatFixed2Signed(state.temperature_c_x100, temp, sizeof(temp));
    formatFixed2(state.humidity_x100, humidity, sizeof(humidity));

    Esp32BaseWeb::sendHeader("Environment");
    Esp32BaseWeb::sendPageTitle("Environment", "Reads outdoor temperature and humidity from the SHT30 on the ESP32 I2C bus.");

    Esp32BaseWeb::beginMetricGrid();
    Esp32BaseWeb::sendMetric("Sensor", state.enabled ? (state.ready ? "ready" : "not ready") : "disabled", state.last_error);
    Esp32BaseWeb::sendMetric("Temperature", state.valid ? temp : "-", "C");
    Esp32BaseWeb::sendMetric("Humidity", state.valid ? humidity : "-", "%RH");
    snprintf(value, sizeof(value), "0x%02x", state.address);
    Esp32BaseWeb::sendMetric("Address", value);
    snprintf(value, sizeof(value), "%d / %d", state.sda_pin, state.scl_pin);
    Esp32BaseWeb::sendMetric("SDA / SCL", value);
    snprintf(value, sizeof(value), "%lu / %lu",
             static_cast<unsigned long>(state.success_count),
             static_cast<unsigned long>(state.fail_count));
    Esp32BaseWeb::sendMetric("Read ok/fail", value);
    Esp32BaseWeb::endMetricGrid();

    Esp32BaseWeb::beginPanel("Read");
    Esp32BaseWeb::sendChunk("<form method='post' action='/api/env/read-now' onsubmit='return once(this)'><div class='actions'><input type='submit' value='Read now'></div></form>");
    Esp32BaseWeb::sendChunk("<div class='tablewrap'><table class='kv'><tbody>");
    Esp32BaseWeb::sendChunk("<tr><th>Last sample</th><td>");
    sendNumber(state.sampled_at_s);
    Esp32BaseWeb::sendChunk("</td></tr><tr><th>Interval</th><td>");
    sendNumber(state.interval_ms);
    Esp32BaseWeb::sendChunk(" ms</td></tr><tr><th>Record interval</th><td>");
    sendNumber(state.record_interval_s);
    Esp32BaseWeb::sendChunk(" s</td></tr><tr><th>Last status</th><td>");
    Esp32BaseWeb::writeHtmlEscaped(state.last_error);
    Esp32BaseWeb::sendChunk("</td></tr></tbody></table></div>");
    Esp32BaseWeb::endPanel();

    Esp32BaseWeb::sendInfoRowCompactLink("Environment settings", "SHT30 I2C pins, address and sampling interval are stored by App Config.", "App Config", "/esp32base/app-config", "Edit", Esp32BaseWeb::UI_INFO);
    Esp32BaseWeb::sendInfoRowCompactLink("Environment records", "Periodic SHT30 samples are stored in Esp32Base App Events.", "App Events", "/esp32base/app-events?source=farm&type=env_sample", "Open", Esp32BaseWeb::UI_INFO);
    Esp32BaseWeb::sendFooter();
}

void sendEnvReadNowApi(void) {
    if (!Esp32BaseWeb::checkPostAllowed("env_read_now")) {
        return;
    }
    if (g_env_sensor == nullptr) {
        Esp32BaseWeb::sendJson(503, "{\"ok\":false,\"error\":\"service_unavailable\"}");
        return;
    }
    const bool ok = g_env_sensor->readNow();
    const FaEnvSensorSnapshot state = g_env_sensor->snapshot();
    Esp32BaseWeb::beginJson(ok ? 200 : 503);
    Esp32BaseWeb::sendChunk("\"ok\":");
    Esp32BaseWeb::sendChunk(ok ? "true" : "false");
    Esp32BaseWeb::sendChunk(",\"temperatureCX100\":");
    sendSignedNumber(state.temperature_c_x100);
    Esp32BaseWeb::sendChunk(",\"humidityX100\":");
    sendNumber(state.humidity_x100);
    Esp32BaseWeb::sendChunk(",\"error\":\"");
    Esp32BaseWeb::writeJsonEscaped(state.last_error);
    Esp32BaseWeb::sendChunk("\"");
    Esp32BaseWeb::endJson();
}
