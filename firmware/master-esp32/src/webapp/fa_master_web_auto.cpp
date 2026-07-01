#include "fa_master_web_internal.h"

namespace {

void formatMinute(uint16_t minute, char* out, size_t len) {
    if (out == nullptr || len == 0u) {
        return;
    }
    snprintf(out, len, "%02u:%02u", minute / 60u, minute % 60u);
}

void formatPauseUntil(uint32_t epoch, char* out, size_t len) {
    if (out == nullptr || len == 0u) {
        return;
    }
    if (epoch == 0u) {
        snprintf(out, len, "未暂停");
        return;
    }
    if (!Esp32BaseTime::formatEpoch(epoch, out, len)) {
        snprintf(out, len, "%lu", static_cast<unsigned long>(epoch));
    }
}

void sendScheduleRow(const char* name, const char* enabled, uint16_t minute, uint32_t amount, const char* unit) {
    char timeText[8];
    formatMinute(minute, timeText, sizeof(timeText));
    Esp32BaseWeb::sendChunk("<tr><td>");
    Esp32BaseWeb::writeHtmlEscaped(name);
    Esp32BaseWeb::sendChunk("</td><td>");
    Esp32BaseWeb::writeHtmlEscaped(enabled);
    Esp32BaseWeb::sendChunk("</td><td>");
    Esp32BaseWeb::writeHtmlEscaped(timeText);
    Esp32BaseWeb::sendChunk("</td><td>");
    sendNumber(amount);
    Esp32BaseWeb::sendChunk(" ");
    Esp32BaseWeb::writeHtmlEscaped(unit);
    Esp32BaseWeb::sendChunk("</td></tr>");
}

bool setPauseUntil(const char* key, uint32_t until) {
    return Esp32BaseConfig::setInt(FaAutoScheduleConfig::NS, key, static_cast<int32_t>(until));
}

void sendPauseApi(const char* guard, const char* key, const char* scope) {
    if (!Esp32BaseWeb::checkPostAllowed(guard)) {
        return;
    }
    const Esp32BaseTime::Snapshot now = Esp32BaseTime::snapshot();
    if (!now.synced) {
        Esp32BaseWeb::sendJson(409, "{\"ok\":false,\"error\":\"time_not_synced\"}");
        return;
    }
    uint32_t durationMin = readUIntParam("durationMin", 360u);
    if (durationMin == 0u) {
        durationMin = 360u;
    }
    if (durationMin > 43200u) {
        durationMin = 43200u;
    }
    const uint32_t until = now.epochSec + durationMin * 60u;
    if (!setPauseUntil(key, until)) {
        Esp32BaseWeb::sendJson(500, "{\"ok\":false,\"error\":\"save_failed\"}");
        return;
    }
    ESP32BASE_LOG_I("farm", "auto_%s_paused duration_min=%lu until=%lu",
                    scope,
                    static_cast<unsigned long>(durationMin),
                    static_cast<unsigned long>(until));
    Esp32BaseWeb::beginJson(200);
    Esp32BaseWeb::sendChunk("\"ok\":true,\"scope\":\"");
    Esp32BaseWeb::sendChunk(scope);
    Esp32BaseWeb::sendChunk("\",\"pauseUntil\":");
    sendNumber(until);
    Esp32BaseWeb::sendChunk(",\"message\":\"automatic schedule paused\"");
    Esp32BaseWeb::endJson();
}

void sendResumeApi(const char* guard, const char* key, const char* scope) {
    if (!Esp32BaseWeb::checkPostAllowed(guard)) {
        return;
    }
    if (!setPauseUntil(key, 0u)) {
        Esp32BaseWeb::sendJson(500, "{\"ok\":false,\"error\":\"save_failed\"}");
        return;
    }
    ESP32BASE_LOG_I("farm", "auto_%s_resumed", scope);
    Esp32BaseWeb::beginJson(200);
    Esp32BaseWeb::sendChunk("\"ok\":true,\"scope\":\"");
    Esp32BaseWeb::sendChunk(scope);
    Esp32BaseWeb::sendChunk("\",\"pauseUntil\":0,\"message\":\"automatic schedule resumed\"");
    Esp32BaseWeb::endJson();
}

}  // namespace

void sendAutoPage(void) {
    if (!Esp32BaseWeb::checkAuth()) {
        return;
    }

    const FaAutoScheduleState state = g_auto_scheduler != nullptr ? g_auto_scheduler->snapshot() : FaAutoScheduleState();
    char value[40];
    char feedPause[32];
    char doorPause[32];
    char localTime[8];
    formatPauseUntil(state.feed_pause_until, feedPause, sizeof(feedPause));
    formatPauseUntil(state.door_pause_until, doorPause, sizeof(doorPause));
    formatMinute(state.local_minute, localTime, sizeof(localTime));

    Esp32BaseWeb::sendHeader("自动");
    Esp32BaseWeb::sendPageTitle("自动计划", "只有真实时间已同步时，才会执行每日下料和门控计划。");

    Esp32BaseWeb::beginMetricGrid();
    Esp32BaseWeb::sendMetric("时间", state.time_synced ? "已同步" : "未同步");
    Esp32BaseWeb::sendMetric("本地时间", state.time_synced ? localTime : "-");
    Esp32BaseWeb::sendMetric("自动", uiEnabled(state.enabled));
    Esp32BaseWeb::sendMetric("下料", uiEnabled(state.feed_enabled), feedPause);
    Esp32BaseWeb::sendMetric("门控", uiEnabled(state.door_enabled), doorPause);
    Esp32BaseWeb::sendMetric("动作", uiActionState(g_action_runtime != nullptr && g_action_runtime->isBusy()));
    Esp32BaseWeb::endMetricGrid();

    Esp32BaseWeb::beginPanel("每日计划");
    Esp32BaseWeb::sendChunk("<div class='tablewrap'><table class='evtable'><thead><tr><th>项目</th><th>状态</th><th>时间</th><th>数量</th></tr></thead><tbody>");
    sendScheduleRow("下料 1", uiEnabled(state.feed_enabled), state.feed_1_minute, state.feed_1_amount_mg, "mg");
    sendScheduleRow("下料 2", uiEnabled(state.feed_enabled), state.feed_2_minute, state.feed_2_amount_mg, "mg");
    sendScheduleRow("开门", uiEnabled(state.door_enabled), state.door_open_minute, 0u, "脉冲");
    sendScheduleRow("关门", uiEnabled(state.door_enabled), state.door_close_minute, 0u, "脉冲");
    Esp32BaseWeb::sendChunk("</tbody></table></div>");
    Esp32BaseWeb::endPanel();

    Esp32BaseWeb::beginPanel("暂停自动执行");
    Esp32BaseWeb::sendChunk("<div class='fieldgrid'>");
    Esp32BaseWeb::sendChunk("<form method='post' action='/api/auto/feed-pause' onsubmit='return once(this)' class='field med'><label>暂停下料</label><input type='number' name='durationMin' min='1' max='43200' value='360'><small>从现在开始的分钟数。</small><div class='actions'><input type='submit' value='暂停下料'></div></form>");
    Esp32BaseWeb::sendChunk("<form method='post' action='/api/auto/door-pause' onsubmit='return once(this)' class='field med'><label>暂停门控</label><input type='number' name='durationMin' min='1' max='43200' value='360'><small>从现在开始的分钟数。</small><div class='actions'><input type='submit' value='暂停门控'></div></form>");
    Esp32BaseWeb::sendChunk("</div><div class='actions'>");
    Esp32BaseWeb::sendChunk("<form method='post' action='/api/auto/feed-resume' onsubmit='return once(this)'><input type='submit' value='恢复下料'></form>");
    Esp32BaseWeb::sendChunk("<form method='post' action='/api/auto/door-resume' onsubmit='return once(this)'><input type='submit' value='恢复门控'></form>");
    Esp32BaseWeb::sendChunk("</div>");
    Esp32BaseWeb::endPanel();

    sendActiveActionPanel();
    sendRecentRecordsPanel();

    snprintf(value, sizeof(value), "第 %ld 天", static_cast<long>(state.local_day));
    Esp32BaseWeb::sendInfoRowCompactLink("计划设置", value, "配置", "/esp32base/app-config", "修改", Esp32BaseWeb::UI_INFO);
    Esp32BaseWeb::sendFooter();
}

void sendAutoFeedPauseApi(void) {
    sendPauseApi("auto_feed_pause", FaAutoScheduleConfig::KEY_FEED_PAUSE_UNTIL, "feed");
}

void sendAutoFeedResumeApi(void) {
    sendResumeApi("auto_feed_resume", FaAutoScheduleConfig::KEY_FEED_PAUSE_UNTIL, "feed");
}

void sendAutoDoorPauseApi(void) {
    sendPauseApi("auto_door_pause", FaAutoScheduleConfig::KEY_DOOR_PAUSE_UNTIL, "door");
}

void sendAutoDoorResumeApi(void) {
    sendResumeApi("auto_door_resume", FaAutoScheduleConfig::KEY_DOOR_PAUSE_UNTIL, "door");
}
