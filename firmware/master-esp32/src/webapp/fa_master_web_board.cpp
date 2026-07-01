#include "fa_master_web_internal.h"

#include <string.h>

namespace {

void sendBoolText(bool value) {
    Esp32BaseWeb::sendChunk(value ? "是" : "否");
}

const char* boardButtonRoleText(uint8_t role) {
    switch (role) {
    case FA_BOARD_BUTTON_BOOT:
        return "BOOT";
    case FA_BOARD_BUTTON_RESERVED:
        return "预留按钮";
    case FA_BOARD_BUTTON_OPEN:
        return "开门按钮";
    case FA_BOARD_BUTTON_CLOSE:
        return "关门按钮";
    case FA_BOARD_BUTTON_STOP:
        return "停止按钮";
    default:
        return "未知按钮";
    }
}

const char* boardEventText(const char* event) {
    if (event == nullptr || event[0] == '\0') {
        return "-";
    }
    if (strcmp(event, "configured") == 0) {
        return "配置已加载";
    }
    if (strcmp(event, "boot_press") == 0) {
        return "BOOT 短按";
    }
    if (strcmp(event, "boot_long") == 0) {
        return "BOOT 长按";
    }
    if (strcmp(event, "reserved_press") == 0) {
        return "预留按钮短按";
    }
    if (strcmp(event, "reserved_long") == 0) {
        return "预留按钮长按";
    }
    if (strcmp(event, "door_open_press") == 0) {
        return "开门按钮短按";
    }
    if (strcmp(event, "door_open_long") == 0) {
        return "开门按钮长按";
    }
    if (strcmp(event, "door_close_press") == 0) {
        return "关门按钮短按";
    }
    if (strcmp(event, "door_close_long") == 0) {
        return "关门按钮长按";
    }
    if (strcmp(event, "door_stop_press") == 0) {
        return "停止按钮短按";
    }
    if (strcmp(event, "door_stop_long") == 0) {
        return "停止按钮长按";
    }
    return "未知事件";
}

void sendBoardButtonRow(uint8_t role, const FaBoardButtonSnapshot& button) {
    Esp32BaseWeb::sendChunk("<tr><td>");
    Esp32BaseWeb::writeHtmlEscaped(boardButtonRoleText(role));
    Esp32BaseWeb::sendChunk("</td><td>");
    if (button.configured) {
        sendNumber(static_cast<uint32_t>(button.pin));
    } else {
        Esp32BaseWeb::sendChunk("-");
    }
    Esp32BaseWeb::sendChunk("</td><td>");
    sendBoolText(button.pressed);
    Esp32BaseWeb::sendChunk("</td><td>");
    sendBoolText(button.long_pressed);
    Esp32BaseWeb::sendChunk("</td><td>");
    sendNumber(button.press_count);
    Esp32BaseWeb::sendChunk("</td><td>");
    sendNumber(button.long_press_count);
    Esp32BaseWeb::sendChunk("</td></tr>");
}

}  // namespace

void sendBoardPage(void) {
    if (!Esp32BaseWeb::checkAuth()) {
        return;
    }

    const FaBoardIoSnapshot state = g_board_io != nullptr ? g_board_io->snapshot() : FaBoardIoSnapshot();
    char value[32];

    Esp32BaseWeb::sendHeader("主控板");
    Esp32BaseWeb::sendPageTitle("主控板 IO", "读取主控板按钮，并用真实 ESP32 GPIO 驱动 RUN/ERR 指示灯。");

    Esp32BaseWeb::beginMetricGrid();
    Esp32BaseWeb::sendMetric("主控板 IO", uiEnabled(state.enabled));
    snprintf(value, sizeof(value), "%d / %d", state.run_led_pin, state.err_led_pin);
    Esp32BaseWeb::sendMetric("RUN / ERR 灯", value, state.led_active_low ? "低电平点亮" : "高电平点亮");
    snprintf(value, sizeof(value), "%s / %s", state.run_led_on ? "亮" : "灭", state.err_led_on ? "亮" : "灭");
    Esp32BaseWeb::sendMetric("LED 状态", value);
    snprintf(value, sizeof(value), "%u / %u",
             static_cast<unsigned>(state.debounce_ms),
             static_cast<unsigned>(state.long_press_ms));
    Esp32BaseWeb::sendMetric("消抖 / 长按", value, "ms");
    Esp32BaseWeb::sendMetric("最近事件", boardEventText(state.last_event));
    Esp32BaseWeb::endMetricGrid();

    Esp32BaseWeb::beginPanel("按钮");
    Esp32BaseWeb::sendChunk("<div class='tablewrap'><table class='evtable'><thead><tr><th>用途</th><th>引脚</th><th>按下</th><th>长按</th><th>按下次数</th><th>长按次数</th></tr></thead><tbody>");
    for (uint8_t i = 0u; i < FA_BOARD_BUTTON_COUNT; ++i) {
        sendBoardButtonRow(i, state.buttons[i]);
    }
    Esp32BaseWeb::sendChunk("</tbody></table></div>");
    Esp32BaseWeb::endPanel();

    Esp32BaseWeb::sendInfoRowCompactLink("主控板设置",
                                         "LED 引脚、BOOT 引脚和外接按钮引脚保存在配置页。本阶段只显示按钮事件，尚未把按钮绑定到门控动作。",
                                         "配置",
                                         "/esp32base/app-config",
                                         "修改",
                                         Esp32BaseWeb::UI_INFO);
    Esp32BaseWeb::sendFooter();
}
