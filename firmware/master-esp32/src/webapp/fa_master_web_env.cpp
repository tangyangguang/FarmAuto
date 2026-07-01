#include "fa_master_web_internal.h"

#include <string.h>

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

const char* envStatusText(const char* error) {
    if (error == nullptr || error[0] == '\0') {
        return "未知";
    }
    if (strcmp(error, "ok") == 0) {
        return "正常";
    }
    if (strcmp(error, "disabled") == 0) {
        return "已停用";
    }
    if (strcmp(error, "not_read") == 0) {
        return "尚未读取";
    }
    if (strcmp(error, "wire_begin") == 0) {
        return "I2C 初始化失败";
    }
    if (strcmp(error, "i2c_write") == 0) {
        return "I2C 写入失败";
    }
    if (strcmp(error, "i2c_read") == 0) {
        return "I2C 读取失败";
    }
    if (strcmp(error, "crc") == 0) {
        return "校验失败";
    }
    return "未知状态";
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

    Esp32BaseWeb::sendHeader("温湿度");
    Esp32BaseWeb::sendPageTitle("温湿度", "通过 ESP32 I2C 总线读取 SHT30 室外温湿度。");

    Esp32BaseWeb::beginMetricGrid();
    Esp32BaseWeb::sendMetric("传感器", state.enabled ? (state.ready ? "就绪" : "未就绪") : "停用", envStatusText(state.last_error));
    Esp32BaseWeb::sendMetric("温度", state.valid ? temp : "-", "C");
    Esp32BaseWeb::sendMetric("湿度", state.valid ? humidity : "-", "%RH");
    snprintf(value, sizeof(value), "0x%02x", state.address);
    Esp32BaseWeb::sendMetric("地址", value);
    snprintf(value, sizeof(value), "%d / %d", state.sda_pin, state.scl_pin);
    Esp32BaseWeb::sendMetric("SDA / SCL", value);
    snprintf(value, sizeof(value), "%lu / %lu",
             static_cast<unsigned long>(state.success_count),
             static_cast<unsigned long>(state.fail_count));
    Esp32BaseWeb::sendMetric("读取成功/失败", value);
    Esp32BaseWeb::endMetricGrid();

    Esp32BaseWeb::beginPanel("读取");
    Esp32BaseWeb::sendChunk("<form method='post' action='/api/env/read-now' onsubmit='return once(this)'><div class='actions'><input type='submit' value='立即读取'></div></form>");
    Esp32BaseWeb::sendChunk("<div class='tablewrap'><table class='kv'><tbody>");
    Esp32BaseWeb::sendChunk("<tr><th>最近采样</th><td>");
    sendNumber(state.sampled_at_s);
    Esp32BaseWeb::sendChunk("</td></tr><tr><th>采样间隔</th><td>");
    sendNumber(state.interval_ms);
    Esp32BaseWeb::sendChunk(" ms</td></tr><tr><th>记录间隔</th><td>");
    sendNumber(state.record_interval_s);
    Esp32BaseWeb::sendChunk(" s</td></tr><tr><th>最近状态</th><td>");
    Esp32BaseWeb::writeHtmlEscaped(envStatusText(state.last_error));
    Esp32BaseWeb::sendChunk("</td></tr></tbody></table></div>");
    Esp32BaseWeb::endPanel();

    Esp32BaseWeb::sendInfoRowCompactLink("温湿度设置", "SHT30 I2C 引脚、地址和采样间隔保存在配置页。", "配置", "/esp32base/app-config", "修改", Esp32BaseWeb::UI_INFO);
    Esp32BaseWeb::sendInfoRowCompactLink("温湿度记录", "周期性 SHT30 采样保存在 Esp32Base 应用事件中。", "应用事件", "/esp32base/app-events?source=farm&type=env_sample", "打开", Esp32BaseWeb::UI_INFO);
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
