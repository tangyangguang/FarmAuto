#include "fa_master_web_internal.h"

#include <string.h>

namespace {

enum class V3Page : uint8_t {
    Home,
    Auto,
    Manual,
    Records,
    Settings
};

const char* V3_CSS =
    ":root{--bg:#f5f7fa;--surface:#fff;--surface2:#eef3f7;--line:#dbe4ec;--text:#17212b;--muted:#667789;--green:#0f766e;--green2:#e7f7f4;--blue:#1d5f7a;--blue2:#e8f3f7;--amber:#9b5d12;--amber2:#fff6e5;--red:#b42318;--red2:#fff0ee;--radius:8px;color-scheme:light;font-family:-apple-system,BlinkMacSystemFont,'Segoe UI','PingFang SC','Microsoft YaHei',sans-serif}"
    "*{box-sizing:border-box}html{background:var(--bg);color:var(--text)}body{margin:0;min-height:100vh;background:linear-gradient(180deg,rgba(15,118,110,.05),transparent 230px),linear-gradient(90deg,rgba(29,95,122,.035) 1px,transparent 1px),linear-gradient(180deg,rgba(29,95,122,.025) 1px,transparent 1px),var(--bg);background-size:auto,28px 28px,28px 28px,auto;letter-spacing:0}button,input,select{font:inherit}a{color:inherit;text-decoration:none}.app-shell{width:min(1180px,100%);margin:0 auto;padding:0 12px 92px}.topbar{position:sticky;top:0;z-index:20;display:flex;align-items:center;justify-content:space-between;gap:12px;margin:0 -12px;padding:10px 12px;background:rgba(245,247,250,.94);border-bottom:1px solid rgba(219,228,236,.92);backdrop-filter:blur(16px)}"
    ".brand{display:flex;align-items:center;gap:10px;min-width:0}.brand-mark{display:grid;place-items:center;width:34px;height:34px;flex:0 0 auto;border-radius:var(--radius);background:var(--green);color:#fff;font-size:13px;font-weight:600}.brand-title{margin:0;font-size:17px;line-height:1.1;font-weight:600}.brand-subtitle{display:block;margin-top:3px;color:var(--muted);font-size:12px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}.workspace{display:grid;gap:12px;padding-top:10px}.page-rail{position:sticky;top:65px;z-index:10;display:none;gap:7px;margin:0 -12px;padding:0 12px 10px;overflow-x:auto}.rail-title{display:none}.rail-button,.tab-button,.seg-button{border:1px solid var(--line);background:rgba(255,255,255,.82);color:var(--text);border-radius:var(--radius);min-height:34px;padding:7px 10px;font-size:13px;font-weight:520;cursor:pointer;white-space:nowrap}.rail-button.active,.tab-button.active,.seg-button.active{color:var(--green);border-color:rgba(15,118,110,.34);background:var(--green2)}"
    ".screen-header{display:flex;justify-content:space-between;align-items:flex-start;gap:12px;margin:2px 0 12px}.screen-kicker{margin:0 0 3px;color:var(--green);font-size:12px;font-weight:650}.screen-header h1{margin:0;font-size:26px;line-height:1.08;font-weight:650}.muted{color:var(--muted)}.small{font-size:12px}.screen-header .muted{margin:6px 0 0;font-size:13px;line-height:1.45}.status-pill,.mini-pill,.tag{display:inline-flex;align-items:center;gap:6px;min-height:28px;border-radius:999px;border:1px solid transparent;font-weight:560;white-space:nowrap}.status-pill{padding:5px 9px;font-size:12px}.mini-pill,.tag{padding:5px 8px;font-size:11px}.ok{color:var(--green);background:var(--green2);border-color:rgba(36,107,69,.18)}.warn{color:var(--amber);background:var(--amber2);border-color:rgba(154,92,19,.20)}.bad{color:var(--red);background:var(--red2);border-color:rgba(167,53,47,.20)}.info{color:var(--blue);background:var(--blue2);border-color:rgba(37,99,122,.18)}.dot{width:7px;height:7px;border-radius:50%;background:currentColor}"
    ".hero-status{border:1px solid var(--line);background:linear-gradient(180deg,#fff,rgba(255,255,255,.86));border-radius:var(--radius);box-shadow:0 10px 26px rgba(23,33,43,.07);padding:14px}.hero-main{display:grid;grid-template-columns:1fr;gap:12px}.hero-title{font-size:24px;line-height:1.1;font-weight:680}.hero-copy{margin:6px 0 0;color:var(--muted);font-size:13px}.top-temp-status{display:grid;gap:2px;text-align:left;border:1px solid rgba(15,118,110,.2);background:var(--green2);border-radius:var(--radius);padding:10px;color:var(--green);cursor:pointer}.top-temp-status span{font-size:12px}.top-temp-status strong{font-size:19px}.home-summary-grid,.grid{display:grid;gap:10px}.home-summary-grid{margin-top:12px}.home-summary-card,.card{border:1px solid var(--line);background:var(--surface);border-radius:var(--radius);padding:12px}.summary-head,.section-title,.run-slot-head,.run-task-head,.split-line{display:flex;align-items:flex-start;justify-content:space-between;gap:10px}.summary-title,.event-title,.record-title,.plan-title,.run-task-title{font-weight:650}.summary-main{margin-top:12px;font-size:18px;font-weight:650}.summary-meta,.summary-foot,.event-detail,.record-detail,.plan-detail,.run-task-detail{color:var(--muted);font-size:12px;line-height:1.45}.summary-foot{display:grid;gap:4px;margin-top:12px;padding-top:10px;border-top:1px solid var(--line)}"
    ".run-slot{margin-top:10px}.run-slot h2,.section-title h2,.card h2{margin:0;font-size:17px}.run-task{margin-top:10px;border:1px solid var(--line);border-radius:var(--radius);padding:10px;background:#fff}.run-task.active{background:var(--amber2);border-color:rgba(154,92,19,.22)}.run-task-title,.run-task-detail{margin:0}.progress-track{height:8px;background:rgba(154,92,19,.16);border-radius:999px;overflow:hidden;margin:10px 0}.progress-fill{height:100%;background:var(--amber);border-radius:999px}.run-task-action{display:flex;justify-content:flex-end}.event-list,.plan-list,.record-list,.menu-grid{display:grid;gap:8px}.section-title{margin:14px 0 8px}.event-row,.record-row,.plan-row,.menu-row,.setting-row{display:grid;grid-template-columns:auto 1fr auto;align-items:center;gap:10px;border:1px solid var(--line);background:#fff;border-radius:var(--radius);padding:10px;text-align:left}.menu-row{width:100%;cursor:pointer}.icon-box{display:grid;place-items:center;width:36px;height:36px;border-radius:var(--radius);background:var(--surface2);color:var(--green);font-weight:650}.event-title,.event-detail,.record-title,.record-detail,.plan-title,.plan-detail{margin:0}.event-time,.record-time{color:var(--soft,#8a98a8);font-size:12px;white-space:nowrap}"
    ".fieldgrid,.form-grid{display:grid;gap:10px}.field{display:grid;gap:5px}.field label{font-size:12px;color:var(--muted);font-weight:650}.field input,.field select{width:100%;min-height:40px;border:1px solid var(--line);border-radius:var(--radius);padding:8px 9px;background:#fff;color:var(--text)}.button-row,.actions{display:flex;flex-wrap:wrap;gap:8px}.primary-button,.secondary-button,.danger-button,input[type=submit],button{border:1px solid var(--line);border-radius:var(--radius);min-height:38px;padding:8px 11px;background:#fff;color:var(--text);font-weight:560;cursor:pointer}.primary-button{background:var(--green);border-color:var(--green);color:#fff}.secondary-button{background:#fff}.danger-button{background:var(--red2);border-color:rgba(180,35,24,.22);color:var(--red)}button:disabled{opacity:.45;cursor:not-allowed}.bottom-nav{position:fixed;left:0;right:0;bottom:0;z-index:30;display:grid;grid-template-columns:repeat(5,1fr);gap:0;background:rgba(255,255,255,.96);border-top:1px solid var(--line);backdrop-filter:blur(16px)}.nav-button{display:grid;gap:2px;place-items:center;border:0;border-radius:0;background:transparent;min-height:58px;padding:7px 4px;color:var(--muted);font-size:12px}.nav-button span:first-child{display:grid;place-items:center;width:22px;height:22px;border-radius:999px;background:var(--surface2);font-size:12px}.nav-button.active{color:var(--green)}.nav-button.active span:first-child{background:var(--green2)}.toast{position:fixed;left:12px;right:12px;bottom:70px;z-index:40;display:none;padding:10px 12px;border-radius:var(--radius);background:#17212b;color:#fff;font-size:13px}.toast.show{display:block}.tablewrap{overflow-x:auto}table{width:100%;border-collapse:collapse}th,td{text-align:left;border-bottom:1px solid var(--line);padding:9px 6px;font-size:13px}th{color:var(--muted);font-size:12px;font-weight:650}"
    "@media(min-width:760px){.app-shell{padding-bottom:24px}.workspace{grid-template-columns:150px 1fr}.page-rail{display:grid;align-self:start;margin:0;padding:0;top:70px;overflow:visible}.rail-title{display:block;color:var(--muted);font-size:12px;font-weight:650;margin-bottom:2px}.rail-button{text-align:left}.bottom-nav{display:none}.hero-main{grid-template-columns:1fr 220px}.home-summary-grid,.desktop-two{grid-template-columns:repeat(2,minmax(0,1fr))}.menu-grid{grid-template-columns:repeat(2,minmax(0,1fr))}.fieldgrid{grid-template-columns:repeat(3,minmax(0,1fr))}}";

void sendEsc(const char* text) {
    Esp32BaseWeb::writeHtmlEscaped(text != nullptr ? text : "");
}

void sendU32(uint32_t value) {
    sendNumber(value);
}

void formatMinuteText(uint16_t minute, char* out, size_t len) {
    snprintf(out, len, "%02u:%02u", static_cast<unsigned>(minute / 60u), static_cast<unsigned>(minute % 60u));
}

void formatEnvValue(const FaEnvSensorSnapshot& env, char* out, size_t len) {
    if (!env.valid) {
        snprintf(out, len, "-");
        return;
    }
    snprintf(out, len, "%ld.%02ld°C · %lu%%",
             static_cast<long>(env.temperature_c_x100 / 100),
             static_cast<long>(env.temperature_c_x100 >= 0 ? env.temperature_c_x100 % 100 : -(env.temperature_c_x100 % 100)),
             static_cast<unsigned long>(env.humidity_x100 / 100u));
}

const char* navHref(V3Page page) {
    switch (page) {
    case V3Page::Home:
        return "/home";
    case V3Page::Auto:
        return "/auto";
    case V3Page::Manual:
        return "/manual";
    case V3Page::Records:
        return "/records";
    case V3Page::Settings:
    default:
        return "/settings";
    }
}

const char* navLabel(V3Page page) {
    switch (page) {
    case V3Page::Home:
        return "首页";
    case V3Page::Auto:
        return "自动";
    case V3Page::Manual:
        return "手动";
    case V3Page::Records:
        return "记录";
    case V3Page::Settings:
    default:
        return "设置";
    }
}

const char* navIcon(V3Page page) {
    switch (page) {
    case V3Page::Home:
        return "首";
    case V3Page::Auto:
        return "自";
    case V3Page::Manual:
        return "手";
    case V3Page::Records:
        return "记";
    case V3Page::Settings:
    default:
        return "设";
    }
}

void sendNavLink(V3Page page, V3Page active) {
    Esp32BaseWeb::sendChunk("<a class='rail-button");
    if (page == active) {
        Esp32BaseWeb::sendChunk(" active");
    }
    Esp32BaseWeb::sendChunk("' href='");
    Esp32BaseWeb::sendChunk(navHref(page));
    Esp32BaseWeb::sendChunk("'>");
    Esp32BaseWeb::sendChunk(navLabel(page));
    Esp32BaseWeb::sendChunk("</a>");
}

void sendBottomNavLink(V3Page page, V3Page active) {
    Esp32BaseWeb::sendChunk("<a class='nav-button");
    if (page == active) {
        Esp32BaseWeb::sendChunk(" active");
    }
    Esp32BaseWeb::sendChunk("' href='");
    Esp32BaseWeb::sendChunk(navHref(page));
    Esp32BaseWeb::sendChunk("'><span>");
    Esp32BaseWeb::sendChunk(navIcon(page));
    Esp32BaseWeb::sendChunk("</span><span>");
    Esp32BaseWeb::sendChunk(navLabel(page));
    Esp32BaseWeb::sendChunk("</span></a>");
}

void sendV3Header(V3Page active, const char* title, const char* subtitle, const char* topText, const char* toneClass = "ok") {
    if (!Esp32BaseWeb::checkAuth()) {
        return;
    }
    if (!Esp32BaseWeb::beginResponse(200, "text/html; charset=utf-8", nullptr)) {
        return;
    }
    Esp32BaseWeb::sendChunk("<!doctype html><html lang='zh-CN'><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'><title>");
    sendEsc(title);
    Esp32BaseWeb::sendChunk("</title><style>");
    Esp32BaseWeb::sendChunk(V3_CSS);
    Esp32BaseWeb::sendChunk("</style><script>"
                            "function faToast(t){var e=document.getElementById('toast');if(!e)return;e.textContent=t||'已发送';e.classList.add('show');setTimeout(function(){e.classList.remove('show')},2600)}"
                            "function faPost(f){if(!window.fetch)return true;if(f.dataset.busy)return false;f.dataset.busy=1;var b=f.querySelector('[type=submit]');if(b)b.disabled=true;fetch(f.action,{method:'POST',body:new URLSearchParams(new FormData(f)).toString(),headers:{'Content-Type':'application/x-www-form-urlencoded','Accept':'application/json'},credentials:'same-origin'}).then(function(r){return r.json().catch(function(){return {ok:false,error:'返回不是 JSON'}})}).then(function(j){faToast(j.ok?'动作已发送':('失败：'+(j.error||j.status||j.transport||'未知错误')))}).catch(function(){faToast('请求失败')}).finally(function(){delete f.dataset.busy;if(b)b.disabled=false});return false}"
                            "</script></head><body><div class='app-shell'><header class='topbar'><div class='brand'><div class='brand-mark'>FA</div><div><p class='brand-title'>FarmAuto</p><span class='brand-subtitle'>已联网 · 真实主控运行中</span></div></div><div class='top-actions'><span class='status-pill ");
    Esp32BaseWeb::sendChunk(toneClass);
    Esp32BaseWeb::sendChunk("'><span class='dot'></span>");
    sendEsc(topText);
    Esp32BaseWeb::sendChunk("</span></div></header><div class='workspace'><aside class='page-rail'><span class='rail-title'>主导航</span>");
    sendNavLink(V3Page::Home, active);
    sendNavLink(V3Page::Auto, active);
    sendNavLink(V3Page::Manual, active);
    sendNavLink(V3Page::Records, active);
    sendNavLink(V3Page::Settings, active);
    Esp32BaseWeb::sendChunk("</aside><main class='content'><section class='screen active'><div class='screen-header'><div><p class='screen-kicker'>");
    sendEsc(navLabel(active));
    Esp32BaseWeb::sendChunk("</p><h1>");
    sendEsc(title);
    Esp32BaseWeb::sendChunk("</h1><p class='muted'>");
    sendEsc(subtitle);
    Esp32BaseWeb::sendChunk("</p></div><span class='status-pill ");
    Esp32BaseWeb::sendChunk(toneClass);
    Esp32BaseWeb::sendChunk("'><span class='dot'></span>");
    sendEsc(topText);
    Esp32BaseWeb::sendChunk("</span></div>");
}

void sendV3Footer(V3Page active) {
    Esp32BaseWeb::sendChunk("</section></main></div></div><nav class='bottom-nav'>");
    sendBottomNavLink(V3Page::Home, active);
    sendBottomNavLink(V3Page::Auto, active);
    sendBottomNavLink(V3Page::Manual, active);
    sendBottomNavLink(V3Page::Records, active);
    sendBottomNavLink(V3Page::Settings, active);
    Esp32BaseWeb::sendChunk("</nav><div class='toast' id='toast'></div></body></html>");
    Esp32BaseWeb::endResponse();
}

void sendDeviceSummaryCard(const char* type, const FaWebDeviceStatus& status, const char* nextText) {
    char device[40];
    char station[48];
    formatDeviceLabel(status.device_id, device, sizeof(device));
    formatStationStatusLabel(status, station, sizeof(station));
    Esp32BaseWeb::sendChunk("<div class='home-summary-card'><div class='summary-head'><span class='summary-title'>");
    sendEsc(type);
    Esp32BaseWeb::sendChunk("</span><span class='mini-pill ");
    Esp32BaseWeb::sendChunk(status.device_enabled ? "ok" : "warn");
    Esp32BaseWeb::sendChunk("'>");
    Esp32BaseWeb::sendChunk(status.device_enabled ? "自动启用" : "已停用");
    Esp32BaseWeb::sendChunk("</span></div><div><div class='summary-main'>");
    sendEsc(device);
    Esp32BaseWeb::sendChunk("</div><div class='summary-meta'>");
    sendEsc(station);
    Esp32BaseWeb::sendChunk("</div></div><div class='summary-foot'><span>状态：");
    Esp32BaseWeb::sendChunk(uiActionState(g_action_runtime != nullptr && g_action_runtime->isBusy()));
    Esp32BaseWeb::sendChunk("</span><span>");
    sendEsc(nextText);
    Esp32BaseWeb::sendChunk("</span></div></div>");
}

void sendActiveRunSlot(void) {
    const bool busy = g_action_runtime != nullptr && g_action_runtime->isBusy() && g_action_runtime->activeRecord() != nullptr;
    Esp32BaseWeb::sendChunk("<div class='card run-slot'><div class='run-slot-head'><h2>当前执行</h2><span class='tag ");
    Esp32BaseWeb::sendChunk(busy ? "warn'>运行中" : "ok'>空闲");
    Esp32BaseWeb::sendChunk("</span></div>");
    if (busy) {
        const FaActionRecord* active = g_action_runtime->activeRecord();
        char device[40];
        char progress[56];
        formatDeviceLabel(active->device_id, device, sizeof(device));
        const uint32_t pct = active->target_pulses > 0u ? (active->completed_pulses * 100u) / active->target_pulses : 0u;
        snprintf(progress, sizeof(progress), "%lu / %lu 脉冲 · %lu%%",
                 static_cast<unsigned long>(active->completed_pulses),
                 static_cast<unsigned long>(active->target_pulses),
                 static_cast<unsigned long>(pct > 100u ? 100u : pct));
        Esp32BaseWeb::sendChunk("<div class='run-task active'><div class='run-task-head'><div><p class='run-task-title'>");
        sendEsc(device);
        Esp32BaseWeb::sendChunk(" · 正在执行</p><p class='run-task-detail'>");
        sendEsc(progress);
        Esp32BaseWeb::sendChunk("</p></div><span class='tag warn'>运行中</span></div><div class='progress-track'><div class='progress-fill' style='width:");
        sendU32(pct > 100u ? 100u : pct);
        Esp32BaseWeb::sendChunk("%'></div></div><div class='run-task-action'><form method='post' action='/api/action/stop-active' onsubmit='return faPost(this)'><input class='danger-button' type='submit' value='停止当前动作'></form></div></div>");
    } else {
        Esp32BaseWeb::sendChunk("<div class='run-task'><div class='run-task-head'><div><p class='run-task-title'>当前无动作</p><p class='run-task-detail'>空闲时保留当前位置，停止按钮不可用。</p></div><span class='tag ok'>空闲</span></div><div class='run-task-action'><button class='danger-button' disabled>停止当前动作</button></div></div>");
    }
    Esp32BaseWeb::sendChunk("</div>");
}

void sendRecordRows(uint8_t limit) {
    if (!FaActionRecordStore::isReady() || FaActionRecordStore::count() == 0u) {
        Esp32BaseWeb::sendChunk("<div class='record-row'><div class='icon-box'>记</div><div><p class='record-title'>暂无动作记录</p><p class='record-detail'>完成、失败或停止的动作会显示在这里。</p></div><span class='record-time'>-</span></div>");
        return;
    }
    const uint16_t count = FaActionRecordStore::count();
    const uint16_t n = count < limit ? count : limit;
    for (uint16_t i = 0u; i < n; ++i) {
        FaActionRecord record;
        if (!FaActionRecordStore::readLatest(i, record)) {
            continue;
        }
        char device[40];
        char detail[96];
        char time[24];
        formatDeviceLabel(record.device_id, device, sizeof(device));
        formatTimeValue(record.started_at_s, time, sizeof(time));
        snprintf(detail, sizeof(detail), "%s · %lu/%lu 脉冲 · %s",
                 uiRecordState(record.state),
                 static_cast<unsigned long>(record.completed_pulses),
                 static_cast<unsigned long>(record.target_pulses),
                 uiStopReason(record.stop_reason));
        Esp32BaseWeb::sendChunk("<div class='record-row'><div class='icon-box'>");
        Esp32BaseWeb::sendChunk(record.device_id == kSingleDoorDeviceId ? "门" : "料");
        Esp32BaseWeb::sendChunk("</div><div><p class='record-title'>");
        sendEsc(device);
        Esp32BaseWeb::sendChunk("</p><p class='record-detail'>");
        sendEsc(detail);
        Esp32BaseWeb::sendChunk("</p></div><span class='record-time'>");
        sendEsc(time);
        Esp32BaseWeb::sendChunk("</span></div>");
    }
}

}  // namespace

void redirectV3Home(void) {
    Esp32BaseWeb::redirectSeeOther("/home");
}

void redirectV3Manual(void) {
    Esp32BaseWeb::redirectSeeOther("/manual");
}

void redirectV3Records(void) {
    Esp32BaseWeb::redirectSeeOther("/records");
}

void redirectV3Settings(void) {
    Esp32BaseWeb::redirectSeeOther("/settings");
}

void sendV3HomePage(void) {
    FaEnvSensorSnapshot env = g_env_sensor != nullptr ? g_env_sensor->snapshot() : FaEnvSensorSnapshot();
    FaAutoScheduleState autoState = g_auto_scheduler != nullptr ? g_auto_scheduler->snapshot() : FaAutoScheduleState();
    FaFeedDeviceConfig feedConfig = fa_master_read_feed_config();
    FaDoorDeviceConfig doorConfig = fa_master_read_door_config();
    FaWebDeviceStatus feedStatus;
    FaWebDeviceStatus doorStatus;
    (void)readDeviceStatus(FA_DEVICE_TYPE_FEEDER, kSingleFeederDeviceId, feedConfig.station_address, feedStatus);
    (void)readDeviceStatus(FA_DEVICE_TYPE_DOOR, kSingleDoorDeviceId, doorConfig.station_address, doorStatus);
    char envText[40];
    char nextDoor[32];
    char nextFeed[32];
    formatEnvValue(env, envText, sizeof(envText));
    formatMinuteText(autoState.door_close_minute, nextDoor, sizeof(nextDoor));
    formatMinuteText(autoState.feed_2_minute, nextFeed, sizeof(nextFeed));

    sendV3Header(V3Page::Home, "运行总览", "下一次自动动作和最近关键事件集中显示。", "系统正常", "ok");
    Esp32BaseWeb::sendChunk("<div class='hero-status'><div class='hero-main'><div><div class='hero-title'>当前无告警</div><p class='hero-copy'>门控、下料、温湿度和主控 Web 服务正在运行。</p></div><a class='top-temp-status' href='/records'><span>温湿度</span><strong>");
    sendEsc(envText);
    Esp32BaseWeb::sendChunk("</strong><span>SHT30 实时读取</span></a></div><div class='home-summary-grid'>");
    char nextText[48];
    snprintf(nextText, sizeof(nextText), "下次：%s 关门", nextDoor);
    sendDeviceSummaryCard("门控", doorStatus, nextText);
    snprintf(nextText, sizeof(nextText), "下次：%s 下料", nextFeed);
    sendDeviceSummaryCard("下料", feedStatus, nextText);
    Esp32BaseWeb::sendChunk("</div></div>");
    sendActiveRunSlot();
    Esp32BaseWeb::sendChunk("<div class='section-title'><h2>最近关键事件</h2><a class='secondary-button' href='/records'>看全部</a></div><div class='event-list'>");
    sendRecordRows(2);
    Esp32BaseWeb::sendChunk("</div>");
    sendV3Footer(V3Page::Home);
}

void sendV3AutoPage(void) {
    FaAutoScheduleState state = g_auto_scheduler != nullptr ? g_auto_scheduler->snapshot() : FaAutoScheduleState();
    char openTime[8];
    char closeTime[8];
    char feed1Time[8];
    char feed2Time[8];
    formatMinuteText(state.door_open_minute, openTime, sizeof(openTime));
    formatMinuteText(state.door_close_minute, closeTime, sizeof(closeTime));
    formatMinuteText(state.feed_1_minute, feed1Time, sizeof(feed1Time));
    formatMinuteText(state.feed_2_minute, feed2Time, sizeof(feed2Time));

    sendV3Header(V3Page::Auto, "自动计划", "这里只管理每天自动执行的门控和下料计划；手动动作在“手动”里操作。", "自动计划", "ok");
    Esp32BaseWeb::sendChunk("<div class='grid desktop-two'><div class='card'><div class='section-title' style='margin:0 0 10px'><h2>门控计划</h2><span class='tag ");
    Esp32BaseWeb::sendChunk(state.door_enabled ? "ok'>自动启用" : "warn'>已停用");
    Esp32BaseWeb::sendChunk("</span></div><div class='plan-list'><div class='plan-row'><div class='icon-box'>开</div><div><p class='plan-title'>");
    sendEsc(openTime);
    Esp32BaseWeb::sendChunk(" 开门 · 门控 01</p><p class='plan-detail'>每天执行 · 到位后由分站停止</p></div><span class='tag ok'>启用</span></div><div class='plan-row'><div class='icon-box'>关</div><div><p class='plan-title'>");
    sendEsc(closeTime);
    Esp32BaseWeb::sendChunk(" 关门 · 门控 01</p><p class='plan-detail'>每天执行 · 到位后由分站停止</p></div><span class='tag ok'>启用</span></div></div><div class='button-row' style='margin-top:12px'><form method='post' action='/api/auto/door-pause' onsubmit='return faPost(this)'><input type='hidden' name='durationMin' value='360'><input class='danger-button' type='submit' value='暂停自动门控'></form><form method='post' action='/api/auto/door-resume' onsubmit='return faPost(this)'><input class='secondary-button' type='submit' value='恢复门控'></form></div></div>");
    Esp32BaseWeb::sendChunk("<div class='card'><div class='section-title' style='margin:0 0 10px'><h2>下料计划</h2><span class='tag ");
    Esp32BaseWeb::sendChunk(state.feed_enabled ? "ok'>自动启用" : "warn'>已停用");
    Esp32BaseWeb::sendChunk("</span></div><div class='plan-list'><div class='plan-row'><div class='icon-box'>早</div><div><p class='plan-title'>");
    sendEsc(feed1Time);
    Esp32BaseWeb::sendChunk(" 早料</p><p class='plan-detail'>");
    sendU32(state.feed_1_amount_mg);
    Esp32BaseWeb::sendChunk(" mg · 按标定值换算圈数</p></div><span class='tag ok'>启用</span></div><div class='plan-row'><div class='icon-box'>晚</div><div><p class='plan-title'>");
    sendEsc(feed2Time);
    Esp32BaseWeb::sendChunk(" 晚料</p><p class='plan-detail'>");
    sendU32(state.feed_2_amount_mg);
    Esp32BaseWeb::sendChunk(" mg · 按标定值换算圈数</p></div><span class='tag ok'>启用</span></div></div><div class='button-row' style='margin-top:12px'><form method='post' action='/api/auto/feed-pause' onsubmit='return faPost(this)'><input type='hidden' name='durationMin' value='360'><input class='danger-button' type='submit' value='暂停自动下料'></form><form method='post' action='/api/auto/feed-resume' onsubmit='return faPost(this)'><input class='secondary-button' type='submit' value='恢复下料'></form></div></div></div>");
    Esp32BaseWeb::sendChunk("<div class='section-title'><h2>计划参数</h2><a class='secondary-button' href='/esp32base/app-config'>修改配置</a></div>");
    sendV3Footer(V3Page::Auto);
}

void sendV3ManualPage(void) {
    FaFeedDeviceConfig feedConfig = fa_master_read_feed_config();
    FaDoorDeviceConfig doorConfig = fa_master_read_door_config();
    FaWebDeviceStatus feedStatus;
    FaWebDeviceStatus doorStatus;
    (void)readDeviceStatus(FA_DEVICE_TYPE_FEEDER, kSingleFeederDeviceId, feedConfig.station_address, feedStatus);
    (void)readDeviceStatus(FA_DEVICE_TYPE_DOOR, kSingleDoorDeviceId, doorConfig.station_address, doorStatus);
    char feedName[40];
    char doorName[40];
    formatDeviceLabel(feedStatus.device_id, feedName, sizeof(feedName));
    formatDeviceLabel(doorStatus.device_id, doorName, sizeof(doorName));

    sendV3Header(V3Page::Manual, "手动操作", "手动门控和手动下料只选择业务设备，不在这里做地址绑定和删除。", "手动", "ok");
    Esp32BaseWeb::sendChunk("<div class='grid desktop-two'><div class='card'><div class='section-title' style='margin:0 0 10px'><h2>手动门控</h2><span class='tag ok'>");
    sendEsc(doorName);
    Esp32BaseWeb::sendChunk("</span></div><p class='muted small'>开门、关门和停止都会下发到当前绑定的门控分站。</p><div class='button-row'><form method='post' action='/api/door/open' onsubmit='return faPost(this)'><input class='primary-button' type='submit' value='开门'></form><form method='post' action='/api/door/close' onsubmit='return faPost(this)'><input class='primary-button' type='submit' value='关门'></form><form method='post' action='/api/door/stop' onsubmit='return faPost(this)'><input class='danger-button' type='submit' value='停止门控'></form></div></div>");
    Esp32BaseWeb::sendChunk("<div class='card'><div class='section-title' style='margin:0 0 10px'><h2>手动下料</h2><span class='tag ok'>");
    sendEsc(feedName);
    Esp32BaseWeb::sendChunk("</span></div><form method='post' action='/api/feed/manual' onsubmit='return faPost(this)'><div class='fieldgrid'><div class='field'><label>数量</label><input type='number' name='amount' min='1' max='1000000' value='4000'></div><div class='field'><label>模式</label><select name='mode'><option value='mg'>毫克</option><option value='turns'>千分之一圈</option></select></div></div><div class='actions' style='margin-top:12px'><input class='primary-button' type='submit' value='执行下料'></div></form></div></div>");
    sendActiveRunSlot();
    sendV3Footer(V3Page::Manual);
}

void sendV3RecordsPage(void) {
    FaEnvSensorSnapshot env = g_env_sensor != nullptr ? g_env_sensor->snapshot() : FaEnvSensorSnapshot();
    char envText[40];
    formatEnvValue(env, envText, sizeof(envText));
    sendV3Header(V3Page::Records, "记录", "门控、下料、温湿度、系统事件和设备通讯统一按时间倒序查看。", "记录", "ok");
    Esp32BaseWeb::sendChunk("<div class='button-row' style='margin-bottom:10px'><button class='tab-button active'>门控/下料</button><button class='tab-button'>温湿度</button><button class='tab-button'>系统事件</button><button class='tab-button'>设备通讯</button></div><div class='card'><div class='section-title' style='margin:0 0 10px'><h2>温湿度</h2><span class='tag info'>");
    sendEsc(envText);
    Esp32BaseWeb::sendChunk("</span></div><p class='muted small'>SHT30 当前读数来自真实 I2C 采样；历史点保存在 Esp32Base 应用事件中。</p></div><div class='section-title'><h2>最近动作记录</h2></div><div class='record-list'>");
    sendRecordRows(kRecentRecordLimit);
    Esp32BaseWeb::sendChunk("</div>");
    sendV3Footer(V3Page::Records);
}

void sendV3SettingsPage(void) {
    sendV3Header(V3Page::Settings, "设置与维护", "这里放系统配置、设备维护和通知规则；日常查看和操作不放在这里。", "配置入口", "ok");
    Esp32BaseWeb::sendChunk("<div class='section-title'><h2>系统配置</h2></div><div class='menu-grid'><a class='menu-row' href='/esp32base/app-config'><span class='icon-box'>参</span><span><span class='event-title'>系统设置</span><span class='event-detail'>RS485 通讯、温湿度记录、自动运行和主控板 IO</span></span></a><a class='menu-row' href='/settings#devices'><span class='icon-box'>设</span><span><span class='event-title'>设备管理</span><span class='event-detail'>按类型查看设备、绑定地址和启用状态</span></span></a><a class='menu-row' href='/settings#scan'><span class='icon-box'>扫</span><span><span class='event-title'>扫描 RS485 地址</span><span class='event-detail'>默认扫描 1-127 地址，在线未绑定地址可用于绑定</span></span></a><a class='menu-row' href='/settings#notify'><span class='icon-box info'>知</span><span><span class='event-title'>通知规则</span><span class='event-detail'>只保存规则，真实发送通道后续接入</span></span></a></div>");
    Esp32BaseWeb::sendChunk("<div class='section-title' id='devices'><h2>设备管理</h2></div><div class='grid desktop-two'>");
    if (g_device_registry != nullptr && g_device_registry->isReady()) {
        for (uint8_t i = 0u; i < g_device_registry->deviceCount(); ++i) {
            FaDeviceRecord device;
            if (!g_device_registry->deviceAt(i, device)) {
                continue;
            }
            FaStationRecord station;
            const bool hasStation = g_device_registry->stationById(device.station_id, station);
            Esp32BaseWeb::sendChunk("<div class='card'><div class='section-title' style='margin:0 0 10px'><h2>");
            sendEsc(device.name);
            Esp32BaseWeb::sendChunk("</h2><span class='tag ");
            Esp32BaseWeb::sendChunk(device.enabled ? "ok'>启用" : "warn'>停用");
            Esp32BaseWeb::sendChunk("</span></div><div class='split-line'><span class='muted'>类型</span><strong>");
            Esp32BaseWeb::sendChunk(device.type == FA_DEVICE_TYPE_DOOR ? "门控" : "下料");
            Esp32BaseWeb::sendChunk("</strong></div><div class='split-line'><span class='muted'>显示顺序</span><strong>");
            sendU32(device.display_no);
            Esp32BaseWeb::sendChunk("</strong></div><div class='split-line'><span class='muted'>RS485 地址</span><strong>");
            if (hasStation) {
                sendU32(station.bus_address);
            } else {
                Esp32BaseWeb::sendChunk("-");
            }
            Esp32BaseWeb::sendChunk("</strong></div><div class='split-line'><span class='muted'>在线状态</span><strong>");
            Esp32BaseWeb::sendChunk(hasStation ? uiStationOnlineState(station.online_state) : "未绑定");
            Esp32BaseWeb::sendChunk("</strong></div></div>");
        }
    } else {
        Esp32BaseWeb::sendChunk("<div class='card'><h2>设备表不可用</h2><p class='muted'>LittleFS 设备表未就绪。</p></div>");
    }
    Esp32BaseWeb::sendChunk("</div><div class='section-title' id='scan'><h2>扫描 RS485 地址</h2></div><div class='card'><form method='post' action='/api/bus/scan' onsubmit='return faPost(this)'><div class='fieldgrid'><div class='field'><label>起始地址</label><input type='number' name='start' min='1' max='127' value='1'></div><div class='field'><label>结束地址</label><input type='number' name='end' min='1' max='127' value='127'></div><div class='field'><label>超时 ms</label><input type='number' name='timeout' min='20' max='2000' value='25'></div></div><div class='actions' style='margin-top:12px'><input class='primary-button' type='submit' value='扫描 RS485 地址'></div></form></div>");
    Esp32BaseWeb::sendChunk("<div class='section-title' id='notify'><h2>通知规则</h2><a class='secondary-button' href='/esp32base/app-config'>修改规则</a></div><div class='card'><p class='muted'>当前只保存通知规则；巴法云或微信真实发送属于后续专项。</p></div>");
    sendV3Footer(V3Page::Settings);
}
