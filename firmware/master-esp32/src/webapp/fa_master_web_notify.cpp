#include "fa_master_web_internal.h"

#include "fa_notification_rules.h"

namespace {

bool ruleEnabled(const char* key, bool fallback) {
    return Esp32BaseConfig::getBool(FaNotificationConfig::NS, key, fallback);
}

void sendRuleRow(const char* event, const char* detail, bool enabled) {
    Esp32BaseWeb::sendChunk("<tr><td>");
    Esp32BaseWeb::writeHtmlEscaped(event);
    Esp32BaseWeb::sendChunk("</td><td>");
    Esp32BaseWeb::writeHtmlEscaped(enabled ? "enabled" : "disabled");
    Esp32BaseWeb::sendChunk("</td><td>");
    Esp32BaseWeb::writeHtmlEscaped(detail);
    Esp32BaseWeb::sendChunk("</td></tr>");
}

}  // namespace

void sendNotifyPage(void) {
    if (!Esp32BaseWeb::checkAuth()) {
        return;
    }

    const bool enabled = ruleEnabled(FaNotificationConfig::KEY_ENABLED, true);
    const bool actionDone = ruleEnabled(FaNotificationConfig::KEY_ACTION_DONE, false);
    const bool actionFailed = ruleEnabled(FaNotificationConfig::KEY_ACTION_FAILED, true);
    const bool stationFault = ruleEnabled(FaNotificationConfig::KEY_STATION_FAULT, true);
    const bool stationOffline = ruleEnabled(FaNotificationConfig::KEY_STATION_OFFLINE, true);
    const bool scheduleSkipped = ruleEnabled(FaNotificationConfig::KEY_SCHEDULE_SKIPPED, true);
    const bool powerRestored = ruleEnabled(FaNotificationConfig::KEY_POWER_RESTORED, true);

    Esp32BaseWeb::sendHeader("Notify");
    Esp32BaseWeb::sendPageTitle("Notification rules", "Rules are stored now; Bafa or WeChat delivery is a later integration.");

    Esp32BaseWeb::beginMetricGrid();
    Esp32BaseWeb::sendMetric("Rules", enabled ? "enabled" : "disabled");
    Esp32BaseWeb::sendMetric("Sender", "not connected", "No external message channel in this build");
    Esp32BaseWeb::sendMetric("Action failed", actionFailed ? "enabled" : "disabled");
    Esp32BaseWeb::sendMetric("Station offline", stationOffline ? "enabled" : "disabled");
    Esp32BaseWeb::endMetricGrid();

    Esp32BaseWeb::beginPanel("Rules");
    Esp32BaseWeb::sendChunk("<div class='tablewrap'><table class='evtable'><thead><tr><th>Event</th><th>Rule</th><th>Meaning</th></tr></thead><tbody>");
    sendRuleRow("Action completed", "Feed or door action reaches terminal completed state.", enabled && actionDone);
    sendRuleRow("Action failed", "Action fails, is stopped by protection, or cannot be tracked.", enabled && actionFailed);
    sendRuleRow("Station fault", "Station returns fault or clear-fault does not remove it.", enabled && stationFault);
    sendRuleRow("Station offline", "Configured station is marked offline by polling or bus operation.", enabled && stationOffline);
    sendRuleRow("Schedule skipped", "Automatic feed or door point is skipped because prerequisites are not ready.", enabled && scheduleSkipped);
    sendRuleRow("Power restored", "Master boots after restart or power recovery.", enabled && powerRestored);
    Esp32BaseWeb::sendChunk("</tbody></table></div>");
    Esp32BaseWeb::endPanel();

    Esp32BaseWeb::sendInfoRowCompactLink("Notification settings", "Delivery channel is intentionally not connected in this build.", "App Config", "/esp32base/app-config", "Edit", Esp32BaseWeb::UI_INFO);
    Esp32BaseWeb::sendFooter();
}
