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
    Esp32BaseWeb::writeHtmlEscaped(uiEnabled(enabled));
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

    Esp32BaseWeb::sendHeader("通知");
    Esp32BaseWeb::sendPageTitle("通知规则", "当前只保存通知规则；巴法云或微信真实发送后续专项接入。");

    Esp32BaseWeb::beginMetricGrid();
    Esp32BaseWeb::sendMetric("规则", uiEnabled(enabled));
    Esp32BaseWeb::sendMetric("发送通道", "未接入", "当前固件未接外部消息通道");
    Esp32BaseWeb::sendMetric("动作失败", uiEnabled(actionFailed));
    Esp32BaseWeb::sendMetric("分站离线", uiEnabled(stationOffline));
    Esp32BaseWeb::endMetricGrid();

    Esp32BaseWeb::beginPanel("规则");
    Esp32BaseWeb::sendChunk("<div class='tablewrap'><table class='evtable'><thead><tr><th>事件</th><th>规则</th><th>含义</th></tr></thead><tbody>");
    sendRuleRow("动作完成", "下料或门控动作进入完成终态。", enabled && actionDone);
    sendRuleRow("动作失败", "动作失败、被保护停止，或主控无法继续跟踪。", enabled && actionFailed);
    sendRuleRow("分站故障", "分站回报故障，或清故障后仍未恢复。", enabled && stationFault);
    sendRuleRow("分站离线", "已配置分站被轮询或总线操作判定为离线。", enabled && stationOffline);
    sendRuleRow("计划跳过", "自动下料或门控计划因前置条件不满足被跳过。", enabled && scheduleSkipped);
    sendRuleRow("上电恢复", "主控重启或断电恢复。", enabled && powerRestored);
    Esp32BaseWeb::sendChunk("</tbody></table></div>");
    Esp32BaseWeb::endPanel();

    Esp32BaseWeb::sendInfoRowCompactLink("通知设置", "当前固件故意不接外部发送通道。", "配置", "/esp32base/app-config", "修改", Esp32BaseWeb::UI_INFO);
    Esp32BaseWeb::sendFooter();
}
