#include "fa_master_web_internal.h"

namespace {

void sendBoolText(bool value) {
    Esp32BaseWeb::sendChunk(value ? "yes" : "no");
}

void sendBoardButtonRow(uint8_t role, const FaBoardButtonSnapshot& button) {
    Esp32BaseWeb::sendChunk("<tr><td>");
    Esp32BaseWeb::writeHtmlEscaped(FaBoardIoService::buttonRoleName(role));
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

    Esp32BaseWeb::sendHeader("Board");
    Esp32BaseWeb::sendPageTitle("Board IO", "Reads master board buttons and drives RUN/ERR LEDs with real ESP32 GPIO.");

    Esp32BaseWeb::beginMetricGrid();
    Esp32BaseWeb::sendMetric("Board IO", state.enabled ? "enabled" : "disabled");
    snprintf(value, sizeof(value), "%d / %d", state.run_led_pin, state.err_led_pin);
    Esp32BaseWeb::sendMetric("RUN / ERR LED", value, state.led_active_low ? "active low" : "active high");
    snprintf(value, sizeof(value), "%s / %s", state.run_led_on ? "on" : "off", state.err_led_on ? "on" : "off");
    Esp32BaseWeb::sendMetric("LED state", value);
    snprintf(value, sizeof(value), "%u / %u",
             static_cast<unsigned>(state.debounce_ms),
             static_cast<unsigned>(state.long_press_ms));
    Esp32BaseWeb::sendMetric("Debounce / long", value, "ms");
    Esp32BaseWeb::sendMetric("Last event", state.last_event[0] != '\0' ? state.last_event : "-");
    Esp32BaseWeb::endMetricGrid();

    Esp32BaseWeb::beginPanel("Buttons");
    Esp32BaseWeb::sendChunk("<div class='tablewrap'><table class='evtable'><thead><tr><th>Role</th><th>Pin</th><th>Pressed</th><th>Long</th><th>Presses</th><th>Long presses</th></tr></thead><tbody>");
    for (uint8_t i = 0u; i < FA_BOARD_BUTTON_COUNT; ++i) {
        sendBoardButtonRow(i, state.buttons[i]);
    }
    Esp32BaseWeb::sendChunk("</tbody></table></div>");
    Esp32BaseWeb::endPanel();

    Esp32BaseWeb::sendInfoRowCompactLink("Board IO settings",
                                         "LED pins, BOOT pin and external button pins are stored by App Config. Button-triggered door actions are not enabled in this step.",
                                         "App Config",
                                         "/esp32base/app-config",
                                         "Edit",
                                         Esp32BaseWeb::UI_INFO);
    Esp32BaseWeb::sendFooter();
}
