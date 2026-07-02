#include "fa_master_web_internal.h"

#include <string.h>

#include "fa_notification_rules.h"

namespace {

enum class V3Page : uint8_t {
    Home,
    Auto,
    Manual,
    Records,
    Settings
};

const char* V3_CSS = R"CSS(
.fa-status-line{display:flex;justify-content:flex-end;margin:-4px 0 10px}
.hero-status{display:grid;gap:10px;padding:14px;border:1px solid var(--eb-line);border-radius:8px;background:rgba(255,255,255,.96)}
.hero-main{display:flex;justify-content:space-between;gap:10px;align-items:center}
.hero-title{margin-bottom:4px;font-size:16px;line-height:1.18;font-weight:750}
.hero-copy{margin:0;color:var(--eb-muted);font-size:13px;line-height:1.5}
.top-temp-status{display:inline-flex;align-items:center;justify-content:flex-end;gap:6px;min-height:30px;padding:5px 8px;border:1px solid var(--eb-line);border-radius:8px;background:var(--eb-soft);color:var(--eb-ink);text-align:right;white-space:nowrap}
.top-temp-status span{color:var(--eb-muted);font-size:12px;line-height:1.2}.top-temp-status strong{font-size:13px;line-height:1.2;font-weight:750}
.fa-grid,.home-summary-grid,.grid,.menu-grid,.event-list,.plan-list{display:grid;gap:10px}.home-summary-grid{margin:10px 0}.desktop-two{display:grid;gap:10px}
.home-summary-card,.card{border:1px solid var(--eb-line);background:var(--eb-surface);border-radius:8px;padding:12px;margin:10px 0}.card h2{font-size:16px;margin:0}.critical-card{border-color:#efc0ba;background:#fffafa}
.summary-head,.section-title,.run-slot-head,.run-task-head,.split-line{display:flex;align-items:flex-start;justify-content:space-between;gap:10px}.section-title{margin:14px 0 8px}.section-title h2{margin:0;font-size:16px}
.summary-title,.event-title,.record-title,.plan-title,.run-task-title{font-weight:700}.summary-main{margin-top:10px;font-size:17px;font-weight:750}.summary-meta,.summary-foot,.event-detail,.record-detail,.plan-detail,.run-task-detail{color:var(--eb-muted);font-size:12px;line-height:1.45}.summary-foot{display:grid;gap:4px;margin-top:10px;padding-top:8px;border-top:1px solid var(--eb-line-soft)}
.mini-pill,.status-pill{display:inline-flex;align-items:center;min-height:24px;padding:2px 8px;border-radius:999px;font-size:11px;font-weight:750}.status-pill{min-height:26px}.dot{display:none}.mini-pill.ok,.status-pill.ok{color:var(--eb-ok);background:var(--eb-ok-soft)}.mini-pill.warn,.status-pill.warn{color:var(--eb-warn);background:var(--eb-warn-soft)}.mini-pill.bad,.status-pill.bad{color:var(--eb-danger);background:var(--eb-danger-soft)}.mini-pill.info,.status-pill.info{color:var(--eb-info);background:var(--eb-info-soft)}
.event-row,.record-row,.plan-row,.menu-row{display:grid;grid-template-columns:auto minmax(0,1fr) auto;align-items:center;gap:10px;border:1px solid var(--eb-line-soft);background:#fff;border-radius:8px;padding:10px;text-align:left;margin:8px 0}.menu-row{width:100%;color:var(--eb-ink)}.icon-box{display:grid;place-items:center;width:34px;height:34px;border-radius:7px;background:var(--eb-primary-soft);color:var(--eb-primary);font-weight:750}.icon-box.info{background:var(--eb-info-soft);color:var(--eb-info)}.event-title,.event-detail,.record-title,.record-detail,.plan-title,.plan-detail{margin:0}.event-time,.record-time{color:var(--eb-muted);font-size:12px;white-space:nowrap}
.button-row{display:flex;flex-wrap:wrap;gap:8px}.button-row form{display:inline-flex}.action-grid{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:7px}.action-grid form{display:grid}.action-grid input[type=submit]{width:100%}.action-button.urgent,input.action-button.urgent{background:var(--eb-danger-soft);color:var(--eb-danger);border-color:#efc0ba}.run-task{margin-top:10px;border:1px solid var(--eb-line-soft);border-radius:8px;padding:10px;background:#fff}.run-task.active{background:var(--eb-warn-soft);border-color:#efcf96}.progress-track{height:8px;background:#f5ddb3;border-radius:999px;overflow:hidden;margin:10px 0}.progress-fill{height:100%;background:var(--eb-warn);border-radius:999px}.run-task-action{display:flex;justify-content:flex-end}
.segmented,.tabs{display:flex;flex-wrap:wrap;gap:6px;margin:8px 0 10px}.seg-button{background:var(--eb-button-soft);color:#344054;border:1px solid var(--eb-button-border)}.seg-button.active{background:var(--eb-primary-soft);color:var(--eb-primary)}.record-panel{display:none}.record-panel.active{display:grid;gap:8px}.record-footer{display:flex;justify-content:space-between;color:var(--eb-muted);font-size:12px}
.form-grid{display:grid;gap:8px}.rule-list{list-style:none;padding:0;margin:0;display:grid;gap:8px}.rule-list li{display:grid;grid-template-columns:auto minmax(0,1fr);gap:8px;align-items:start}.check{display:grid;place-items:center;width:22px;height:22px;border-radius:999px;background:var(--eb-ok-soft);color:var(--eb-ok);font-weight:750}.check.warn-mark{background:var(--eb-warn-soft);color:var(--eb-warn)}
.temp-chart{border:1px solid var(--eb-line);background:#fff;border-radius:8px;padding:12px}.chart-grid{position:relative;height:160px;border:1px solid var(--eb-line);border-radius:8px;background:linear-gradient(180deg,rgba(23,107,122,.05),transparent),repeating-linear-gradient(0deg,transparent,transparent 31px,var(--eb-line-soft) 32px)}.chart-line{position:absolute;height:3px;border-radius:999px;background:var(--eb-primary);transform-origin:left center}.chart-line.hum{background:var(--eb-info)}.chart-dot{position:absolute;width:8px;height:8px;margin:-4px 0 0 -4px;border-radius:50%;background:var(--eb-primary);box-shadow:0 0 0 3px #fff}.chart-dot.hum{background:var(--eb-info)}.chart-legend{display:flex;gap:12px;margin-top:8px;color:var(--eb-muted);font-size:12px}.legend-item{display:inline-flex;align-items:center;gap:6px}.legend-swatch{width:14px;height:3px;border-radius:999px;background:var(--eb-primary)}.legend-swatch.hum{background:var(--eb-info)}
.confirm-sheet{display:none;gap:9px;padding:12px;border-radius:8px;border:1px solid #efc0ba;background:var(--eb-danger-soft);margin:10px 0}.confirm-sheet.show{display:grid;position:fixed;left:14px;right:14px;bottom:14px;z-index:45;box-shadow:0 10px 26px rgba(23,33,43,.12)}.hold-button{position:relative;overflow:hidden;background:var(--eb-danger);border-color:var(--eb-danger);color:#fff;user-select:none;touch-action:manipulation}.hold-button::before{content:'';position:absolute;inset:0;width:var(--hold-progress,0%);background:rgba(255,255,255,.24);transition:width 80ms linear}.hold-button span{position:relative;z-index:1}.toast{position:fixed;left:14px;right:14px;bottom:14px;z-index:40;display:none;padding:10px 12px;border-radius:8px;background:#17212b;color:#fff;font-size:13px}.toast.show{display:block}
@media(min-width:760px){.home-summary-grid,.desktop-two,.menu-grid{grid-template-columns:repeat(2,minmax(0,1fr))}.field{grid-column:span 4}.field.full{grid-column:1/-1}.confirm-sheet.show,.toast{left:50%;right:auto;bottom:22px;width:min(460px,calc(100vw - 28px));transform:translateX(-50%)}}
@media(max-width:520px){.hero-main{align-items:flex-start;flex-direction:column}.top-temp-status{width:100%;justify-content:space-between}.action-grid{grid-template-columns:1fr}.event-row,.record-row,.plan-row,.menu-row{grid-template-columns:auto minmax(0,1fr)}.event-time,.record-time{grid-column:2}}
)CSS";


const char* V3_JS = R"JS(
var faPendingForm=null;
var faHoldTimer=null;
var faHoldProgressTimer=null;
var faSubmitSerial=0;
function faToast(t){
  var e=document.getElementById('toast');
  if(!e)return;
  e.textContent=t||'已处理';
  e.classList.add('show');
  clearTimeout(faToast.timer);
  faToast.timer=setTimeout(function(){e.classList.remove('show')},2600);
}
function faPostNow(f){
  if(f.dataset.busy)return false;
  f.dataset.busy='1';
  var b=f.querySelector('[type=submit]');
  if(b)b.disabled=true;
  var frame=document.getElementById('faSubmitFrame');
  if(!frame)return true;
  var serial=++faSubmitSerial;
  frame.onload=function(){
    if(serial!==faSubmitSerial)return;
    delete f.dataset.busy;
    if(b)b.disabled=false;
    frame.onload=null;
    faToast(f.dataset.success||'动作已发送');
    if(f.dataset.reload==='1')setTimeout(function(){location.reload()},650);
  };
  f.target='faSubmitFrame';
  setTimeout(function(){
    try{f.submit()}catch(e){delete f.dataset.busy;if(b)b.disabled=false;frame.onload=null;faToast('请求失败');}
  },0);
  return false;
}
function faPost(f){
  if(f.dataset.confirmed==='1'){
    delete f.dataset.confirmed;
    return faPostNow(f);
  }
  if(f.dataset.confirmTitle||f.dataset.confirmNote){
    faPendingForm=f;
    document.getElementById('globalConfirmTitle').textContent=f.dataset.confirmTitle||'确认动作';
    document.getElementById('globalConfirmNote').textContent=f.dataset.confirmNote||'该动作会立即发送到主控。';
    document.getElementById('globalConfirm').classList.add('show');
    return false;
  }
  return faPostNow(f);
}
function faCancelConfirm(){
  faPendingForm=null;
  document.getElementById('globalConfirm').classList.remove('show');
  faToast('已取消，未发送命令。');
}
function faHoldStart(ev){
  ev.preventDefault();
  var b=document.getElementById('globalHold');
  var duration=1000;
  var progress=0;
  b.style.setProperty('--hold-progress','0%');
  clearTimeout(faHoldTimer);
  clearInterval(faHoldProgressTimer);
  faHoldProgressTimer=setInterval(function(){
    progress+=80/duration*100;
    b.style.setProperty('--hold-progress',Math.min(progress,100)+'%');
  },80);
  faHoldTimer=setTimeout(function(){
    clearInterval(faHoldProgressTimer);
    b.style.setProperty('--hold-progress','100%');
    document.getElementById('globalConfirm').classList.remove('show');
    if(faPendingForm){
      faPendingForm.dataset.confirmed='1';
      var f=faPendingForm;
      faPendingForm=null;
      faPost(f);
    }
    setTimeout(function(){b.style.setProperty('--hold-progress','0%')},260);
  },duration);
}
function faHoldCancel(){
  clearTimeout(faHoldTimer);
  clearInterval(faHoldProgressTimer);
  var b=document.getElementById('globalHold');
  if(b)b.style.setProperty('--hold-progress','0%');
}
function faRecordTab(name){
  document.querySelectorAll('[data-record-tab]').forEach(function(b){b.classList.toggle('active',b.dataset.recordTab===name)});
  document.querySelectorAll('[data-record-panel]').forEach(function(p){p.classList.toggle('active',p.dataset.recordPanel===name)});
  document.querySelectorAll('[data-record-group]').forEach(function(p){p.classList.toggle('active',p.dataset.recordGroup===name)});
}
document.addEventListener('click',function(ev){
  var t=ev.target.closest('[data-record-tab]');
  if(t){ev.preventDefault();faRecordTab(t.dataset.recordTab);}
  var s=ev.target.closest('input[type=submit]');
  if(s&&s.form&&s.form.getAttribute('onsubmit')&&s.form.getAttribute('onsubmit').indexOf('faPost')>=0){
    ev.preventDefault();
    faPost(s.form);
  }
});
)JS";

void sendEsc(const char* text) {
    Esp32BaseWeb::writeHtmlEscaped(text != nullptr ? text : "");
}

void sendU32(uint32_t value) {
    sendNumber(value);
}

void sendI32(int32_t value) {
    char buf[18];
    snprintf(buf, sizeof(buf), "%ld", static_cast<long>(value));
    Esp32BaseWeb::sendChunk(buf);
}

void sendNumberField(const char* label,
                     const char* name,
                     int32_t value,
                     int32_t min_value,
                     int32_t max_value,
                     int32_t step = 1) {
    Esp32BaseWeb::sendChunk("<div class='field'><label>");
    sendEsc(label);
    Esp32BaseWeb::sendChunk("</label><input type='number' name='");
    Esp32BaseWeb::sendChunk(name);
    Esp32BaseWeb::sendChunk("' min='");
    sendI32(min_value);
    Esp32BaseWeb::sendChunk("' max='");
    sendI32(max_value);
    Esp32BaseWeb::sendChunk("' step='");
    sendI32(step);
    Esp32BaseWeb::sendChunk("' value='");
    sendI32(value);
    Esp32BaseWeb::sendChunk("'></div>");
}

void sendBoolSelect(const char* label, const char* name, bool enabled) {
    Esp32BaseWeb::sendChunk("<div class='field'><label>");
    sendEsc(label);
    Esp32BaseWeb::sendChunk("</label><select name='");
    Esp32BaseWeb::sendChunk(name);
    Esp32BaseWeb::sendChunk("'><option value='1'");
    if (enabled) {
        Esp32BaseWeb::sendChunk(" selected");
    }
    Esp32BaseWeb::sendChunk(">启用</option><option value='0'");
    if (!enabled) {
        Esp32BaseWeb::sendChunk(" selected");
    }
    Esp32BaseWeb::sendChunk(">停用</option></select></div>");
}

void sendDirectionSelect(const char* label, const char* name, int8_t value) {
    Esp32BaseWeb::sendChunk("<div class='field'><label>");
    sendEsc(label);
    Esp32BaseWeb::sendChunk("</label><select name='");
    Esp32BaseWeb::sendChunk(name);
    Esp32BaseWeb::sendChunk("'><option value='1'");
    if (value >= 0) {
        Esp32BaseWeb::sendChunk(" selected");
    }
    Esp32BaseWeb::sendChunk(">正转</option><option value='-1'");
    if (value < 0) {
        Esp32BaseWeb::sendChunk(" selected");
    }
    Esp32BaseWeb::sendChunk(">反转</option></select></div>");
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

void sendV3Header(V3Page active, const char* title, const char* subtitle, const char* topText, const char* toneClass = "ok") {
    (void)active;
    if (!Esp32BaseWeb::checkAuth()) {
        return;
    }
    Esp32BaseWeb::sendHeader(title);
    Esp32BaseWeb::sendChunk("<style>");
    Esp32BaseWeb::sendChunk(V3_CSS);
    Esp32BaseWeb::sendChunk("</style><script>");
    Esp32BaseWeb::sendChunk(V3_JS);
    Esp32BaseWeb::sendChunk("</script>");
    Esp32BaseWeb::sendPageTitle(title, subtitle);
    Esp32BaseWeb::sendChunk("<div class='fa-status-line'><span class='status-pill ");
    Esp32BaseWeb::sendChunk(toneClass);
    Esp32BaseWeb::sendChunk("'><span class='dot'></span>");
    sendEsc(topText);
    Esp32BaseWeb::sendChunk("</span></div>");
}

void sendV3Footer(V3Page active) {
    (void)active;
    Esp32BaseWeb::sendChunk("<iframe id='faSubmitFrame' name='faSubmitFrame' style='display:none;width:0;height:0;border:0'></iframe><div class='confirm-sheet' id='globalConfirm'><div><p class='event-title' id='globalConfirmTitle'>确认动作</p><p class='event-detail' id='globalConfirmNote'></p></div><button class='hold-button' id='globalHold' onmousedown='faHoldStart(event)' ontouchstart='faHoldStart(event)' onmouseup='faHoldCancel()' onmouseleave='faHoldCancel()' ontouchend='faHoldCancel()' ontouchcancel='faHoldCancel()' oncontextmenu='return false'><span>长按 1 秒发送</span></button><button class='secondary' type='button' onclick='faCancelConfirm()'>取消</button></div><div class='toast' id='toast'></div>");
    Esp32BaseWeb::sendFooter();
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
        Esp32BaseWeb::sendChunk("%'></div></div><div class='run-task-action'><form method='post' action='/api/action/stop-active' data-confirm-title='停止当前动作' data-confirm-note='只停止当前正在跟踪的动作，不影响其他空闲设备。' data-success='停止命令已发送' onsubmit='return faPost(this)'><input class='danger' type='submit' value='停止当前动作'></form></div></div>");
    } else {
        Esp32BaseWeb::sendChunk("<div class='run-task'><div class='run-task-head'><div><p class='run-task-title'>当前无动作</p><p class='run-task-detail'>空闲时保留当前位置，停止按钮不可用。</p></div><span class='tag ok'>空闲</span></div><div class='run-task-action'><button class='dangerbtn' disabled>停止当前动作</button></div></div>");
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

void sendRecordRowsForDevice(uint8_t limit,
                             uint16_t device_id,
                             const char* emptyIcon,
                             const char* emptyTitle,
                             const char* emptyDetail) {
    if (!FaActionRecordStore::isReady() || FaActionRecordStore::count() == 0u) {
        Esp32BaseWeb::sendChunk("<div class='record-row'><div class='icon-box'>");
        sendEsc(emptyIcon);
        Esp32BaseWeb::sendChunk("</div><div><p class='record-title'>");
        sendEsc(emptyTitle);
        Esp32BaseWeb::sendChunk("</p><p class='record-detail'>");
        sendEsc(emptyDetail);
        Esp32BaseWeb::sendChunk("</p></div><span class='record-time'>-</span></div>");
        return;
    }
    const uint16_t count = FaActionRecordStore::count();
    uint8_t emitted = 0u;
    for (uint16_t i = 0u; i < count && emitted < limit; ++i) {
        FaActionRecord record;
        if (!FaActionRecordStore::readLatest(i, record) || record.device_id != device_id) {
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
        ++emitted;
    }
    if (emitted == 0u) {
        Esp32BaseWeb::sendChunk("<div class='record-row'><div class='icon-box'>");
        sendEsc(emptyIcon);
        Esp32BaseWeb::sendChunk("</div><div><p class='record-title'>");
        sendEsc(emptyTitle);
        Esp32BaseWeb::sendChunk("</p><p class='record-detail'>");
        sendEsc(emptyDetail);
        Esp32BaseWeb::sendChunk("</p></div><span class='record-time'>-</span></div>");
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
    Esp32BaseWeb::sendChunk("<div class='section-title'><h2>最近关键事件</h2><a class='btnlink secondary' href='/records'>看全部</a></div><div class='event-list'>");
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
    Esp32BaseWeb::sendChunk(" 关门 · 门控 01</p><p class='plan-detail'>每天执行 · 到位后由分站停止</p></div><span class='tag ok'>启用</span></div></div><div class='button-row' style='margin-top:12px'><form method='post' action='/api/auto/door-pause' data-confirm-title='暂停自动门控' data-confirm-note='确认后只暂停自动门控；手动开门、关门、停止仍可使用。' data-success='自动门控已暂停' onsubmit='return faPost(this)'><input type='hidden' name='durationMin' value='360'><input class='danger' type='submit' value='暂停自动门控'></form><form method='post' action='/api/auto/door-resume' data-confirm-title='恢复自动门控' data-confirm-note='恢复后门控会继续按已保存计划自动执行。' data-success='自动门控已恢复' onsubmit='return faPost(this)'><input class='secondary' type='submit' value='恢复门控'></form></div></div>");
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
    Esp32BaseWeb::sendChunk(" mg · 按标定值换算圈数</p></div><span class='tag ok'>启用</span></div></div><div class='button-row' style='margin-top:12px'><form method='post' action='/api/auto/feed-pause' data-confirm-title='暂停自动下料' data-confirm-note='确认后只暂停自动下料；手动下料仍可单独执行。' data-success='自动下料已暂停' onsubmit='return faPost(this)'><input type='hidden' name='durationMin' value='360'><input class='danger' type='submit' value='暂停自动下料'></form><form method='post' action='/api/auto/feed-resume' data-confirm-title='恢复自动下料' data-confirm-note='恢复后下料设备会继续按已保存计划自动执行。' data-success='自动下料已恢复' onsubmit='return faPost(this)'><input class='secondary' type='submit' value='恢复下料'></form></div></div></div>");
    Esp32BaseWeb::sendChunk("<div class='section-title'><h2>计划参数</h2></div><div class='card'><form method='post' action='/api/auto/schedule' data-success='自动计划已保存' data-reload='1' onsubmit='return faPost(this)'><div class='fieldgrid'>");
    sendBoolSelect("总开关", "enabled", state.enabled);
    sendBoolSelect("下料计划", "feedEnabled", state.feed_enabled);
    sendBoolSelect("门控计划", "doorEnabled", state.door_enabled);
    sendNumberField("早料分钟", "feed1Min", state.feed_1_minute, 0, 1439);
    sendNumberField("早料 mg", "feed1AmountMg", static_cast<int32_t>(state.feed_1_amount_mg), 1, 5000000);
    sendNumberField("晚料分钟", "feed2Min", state.feed_2_minute, 0, 1439);
    sendNumberField("晚料 mg", "feed2AmountMg", static_cast<int32_t>(state.feed_2_amount_mg), 1, 5000000);
    sendNumberField("开门分钟", "doorOpenMin", state.door_open_minute, 0, 1439);
    sendNumberField("关门分钟", "doorCloseMin", state.door_close_minute, 0, 1439);
    Esp32BaseWeb::sendChunk("</div><p class='muted small'>分钟数为当天 0-1439，例如 08:00 为 480，17:30 为 1050。</p><div class='actions' style='margin-top:12px'><input type='submit' value='保存自动计划'><a class='btnlink secondary' href='/esp32base/app-config'>高级配置</a></div></form></div>");
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

    sendV3Header(V3Page::Manual, "手动操作", "这里放立即动作。门控按当前门位执行，手动下料需要核对后发送。", "需谨慎", "warn");
    sendActiveRunSlot();
    Esp32BaseWeb::sendChunk("<div class='grid desktop-two'><div class='card'><div class='section-title' style='margin:0 0 10px'><h2>门控手动操作</h2><span class='tag ok'>");
    sendEsc(doorName);
    Esp32BaseWeb::sendChunk("</span></div><div class='split-line'><span class='muted'>当前设备</span><strong>");
    sendEsc(doorName);
    Esp32BaseWeb::sendChunk("</strong></div><div class='split-line'><span class='muted'>执行方式</span><strong>到位后由分站自行停止</strong></div><div class='action-grid' style='margin-top:12px'><form method='post' action='/api/door/open' data-confirm-title='门控手动开门' data-confirm-note='门控设备将执行开门，到位后由分站自行停止。' data-success='开门命令已发送' onsubmit='return faPost(this)'><input class='action-button' type='submit' value='开门'></form><form method='post' action='/api/door/close' data-confirm-title='门控手动关门' data-confirm-note='门控设备将执行关门，到位后由分站自行停止。' data-success='关门命令已发送' onsubmit='return faPost(this)'><input class='action-button' type='submit' value='关门'></form><form method='post' action='/api/door/stop' data-confirm-title='停止门控' data-confirm-note='停止门控当前动作；如果门控空闲，只记录一次停止确认。' data-success='停止门控命令已发送' onsubmit='return faPost(this)'><input class='action-button urgent' type='submit' value='停止'></form></div></div>");
    Esp32BaseWeb::sendChunk("<div class='card critical-card'><div class='section-title' style='margin:0 0 10px'><h2>手动下料</h2><span class='tag ok'>");
    sendEsc(feedName);
    Esp32BaseWeb::sendChunk("</span></div><div class='split-line'><span class='muted'>设备状态</span><strong>");
    sendEsc(feedName);
    Esp32BaseWeb::sendChunk("</strong></div><div class='split-line'><span class='muted'>发送方式</span><strong>选量后长按 1 秒发送</strong></div><form method='post' action='/api/feed/manual' data-confirm-title='确认手动下料' data-confirm-note='确认后立即向下料设备发送动作命令，执行结果会写入记录。' data-success='手动下料命令已发送' onsubmit='return faPost(this)' style='margin-top:12px'><div class='fieldgrid'><div class='field'><label>数量</label><input type='number' name='amount' min='1' max='1000000' value='4000'></div><div class='field'><label>模式</label><select name='mode'><option value='mg'>毫克</option><option value='turns'>千分之一圈</option></select></div></div><div class='actions' style='margin-top:12px'><input type='submit' value='核对并下料'></div></form></div></div>");
    Esp32BaseWeb::sendChunk("<div class='section-title'><h2>操作原则</h2></div><div class='card'><ul class='rule-list'><li><span class='check'>✓</span><span>手动动作只执行一次，结果写入对应记录。</span></li><li><span class='check warn-mark'>!</span><span>暂停自动执行时，手动操作仍允许，但需要单独确认。</span></li><li><span class='check'>✓</span><span>开门、关门和下料命令到达后，由对应分站完成本地闭环和安全停止。</span></li></ul></div>");
    sendV3Footer(V3Page::Manual);
}

void sendV3RecordsPage(void) {
    FaEnvSensorSnapshot env = g_env_sensor != nullptr ? g_env_sensor->snapshot() : FaEnvSensorSnapshot();
    char envText[40];
    formatEnvValue(env, envText, sizeof(envText));
    sendV3Header(V3Page::Records, "分类记录", "门控和下料看业务结果；系统事件看主控、供电、时间和计划恢复；设备通讯看分站、电机、电流和编码器诊断。", "记录正常", "ok");
    Esp32BaseWeb::sendChunk("<div class='segmented' aria-label='记录分类'><button type='button' class='seg-button active' data-record-tab='door'>门控</button><button type='button' class='seg-button' data-record-tab='feed'>下料</button><button type='button' class='seg-button' data-record-tab='temp'>温湿度</button><button type='button' class='seg-button' data-record-tab='system'>系统事件</button><button type='button' class='seg-button' data-record-tab='comm'>设备通讯</button></div>");
    Esp32BaseWeb::sendChunk("<div class='record-panel active' data-record-panel='door'><div class='section-title'><h2>门控记录</h2></div>");
    sendRecordRowsForDevice(kRecentRecordLimit, kSingleDoorDeviceId, "门", "暂无门控记录", "自动开门、关门、手动门控和停止结果会显示在这里。");
    Esp32BaseWeb::sendChunk("<div class='record-footer'><span>按时间倒序</span></div></div>");
    Esp32BaseWeb::sendChunk("<div class='record-panel' data-record-panel='feed'><div class='section-title'><h2>下料记录</h2></div>");
    sendRecordRowsForDevice(kRecentRecordLimit, kSingleFeederDeviceId, "料", "暂无下料记录", "自动下料、手动下料和异常停止结果会显示在这里。");
    Esp32BaseWeb::sendChunk("<div class='record-footer'><span>按时间倒序</span></div></div>");
    Esp32BaseWeb::sendChunk("<div class='record-panel' data-record-panel='temp'><div class='temp-chart'><div class='section-title' style='margin:0'><h2>近 24 小时趋势</h2><span class='tag info'>当前 ");
    sendEsc(envText);
    Esp32BaseWeb::sendChunk("</span></div><div class='segmented' aria-label='温湿度范围'><button type='button' class='seg-button active'>近24小时</button><button type='button' class='seg-button' disabled>近30天</button><button type='button' class='seg-button' disabled>近1年</button></div><div class='chart-grid' aria-label='温湿度趋势图'><span class='chart-line' style='left:5%;top:66%;width:17%;transform:rotate(-16deg)'></span><span class='chart-line' style='left:21%;top:60%;width:18%;transform:rotate(-20deg)'></span><span class='chart-line' style='left:38%;top:50%;width:19%;transform:rotate(-8deg)'></span><span class='chart-line' style='left:56%;top:47%;width:19%;transform:rotate(15deg)'></span><span class='chart-line hum' style='left:5%;top:34%;width:18%;transform:rotate(10deg)'></span><span class='chart-line hum' style='left:22%;top:38%;width:18%;transform:rotate(14deg)'></span><span class='chart-line hum' style='left:39%;top:44%;width:18%;transform:rotate(7deg)'></span><span class='chart-line hum' style='left:56%;top:47%;width:18%;transform:rotate(-12deg)'></span><span class='chart-dot' style='left:5%;top:66%'></span><span class='chart-dot' style='left:22%;top:60%'></span><span class='chart-dot' style='left:39%;top:50%'></span><span class='chart-dot' style='left:57%;top:47%'></span><span class='chart-dot hum' style='left:5%;top:34%'></span><span class='chart-dot hum' style='left:23%;top:38%'></span><span class='chart-dot hum' style='left:40%;top:44%'></span><span class='chart-dot hum' style='left:57%;top:47%'></span></div><div class='chart-legend'><span class='legend-item'><span class='legend-swatch'></span>温度</span><span class='legend-item'><span class='legend-swatch hum'></span>湿度</span></div></div><div class='record-row'><div class='icon-box'>温</div><div><p class='record-title'>当前温湿度记录</p><p class='record-detail'>SHT30 当前读数来自真实 I2C 采样；读数正常时会按记录间隔写入应用事件。</p></div><span class='record-time'>实时</span></div></div>");
    Esp32BaseWeb::sendChunk("<div class='record-panel' data-record-panel='system'><div class='record-row'><div class='icon-box info'>时</div><div><p class='record-title'>系统时间状态</p><p class='record-detail'>联网后通过 NTP 校时，动作记录使用真实时间。</p></div><span class='record-time'>系统</span></div><div class='record-row'><div class='icon-box'>板</div><div><p class='record-title'>主控板状态</p><p class='record-detail'>RUN/ERR 指示灯和按钮输入由真实 GPIO 服务读取。</p></div><span class='record-time'>实时</span></div></div>");
    Esp32BaseWeb::sendChunk("<div class='record-panel' data-record-panel='comm'><div class='record-row'><div class='icon-box'>总</div><div><p class='record-title'>RS485 轮询状态</p><p class='record-detail'>当前通讯模式：");
    Esp32BaseWeb::sendChunk(g_transport != nullptr ? uiTransportMode(g_transport->config().mode) : "不可用");
    Esp32BaseWeb::sendChunk("；设备通讯事件后续会继续归入本分类。</p></div><span class='record-time'>实时</span></div></div>");
    sendV3Footer(V3Page::Records);
}

void sendV3SettingsPage(void) {
    FaFeedDeviceConfig feedConfig = fa_master_read_feed_config();
    FaDoorDeviceConfig doorConfig = fa_master_read_door_config();
    FaEnvSensorSnapshot env = g_env_sensor != nullptr ? g_env_sensor->snapshot() : FaEnvSensorSnapshot();
    char envText[40];
    formatEnvValue(env, envText, sizeof(envText));
    const bool envEnabled = Esp32BaseConfig::getBool(FaEnvSensorConfig::NS, FaEnvSensorConfig::KEY_ENABLED, true);
    const int32_t envAddress = Esp32BaseConfig::getInt(FaEnvSensorConfig::NS, FaEnvSensorConfig::KEY_ADDRESS, 68);
    const int32_t envInterval = Esp32BaseConfig::getInt(FaEnvSensorConfig::NS, FaEnvSensorConfig::KEY_INTERVAL_MS, 5000);
    const int32_t envRecordInterval = Esp32BaseConfig::getInt(FaEnvSensorConfig::NS, FaEnvSensorConfig::KEY_RECORD_INTERVAL_S, 300);
    const bool notifyEnabled = Esp32BaseConfig::getBool(FaNotificationConfig::NS, FaNotificationConfig::KEY_ENABLED, true);
    const bool notifyActionDone = Esp32BaseConfig::getBool(FaNotificationConfig::NS, FaNotificationConfig::KEY_ACTION_DONE, false);
    const bool notifyActionFailed = Esp32BaseConfig::getBool(FaNotificationConfig::NS, FaNotificationConfig::KEY_ACTION_FAILED, true);
    const bool notifyStationFault = Esp32BaseConfig::getBool(FaNotificationConfig::NS, FaNotificationConfig::KEY_STATION_FAULT, true);
    const bool notifyStationOffline = Esp32BaseConfig::getBool(FaNotificationConfig::NS, FaNotificationConfig::KEY_STATION_OFFLINE, true);
    const bool notifyScheduleSkipped = Esp32BaseConfig::getBool(FaNotificationConfig::NS, FaNotificationConfig::KEY_SCHEDULE_SKIPPED, true);
    const bool notifyPowerRestored = Esp32BaseConfig::getBool(FaNotificationConfig::NS, FaNotificationConfig::KEY_POWER_RESTORED, true);
    sendV3Header(V3Page::Settings, "设置与维护", "这里放系统配置、设备维护和通知规则；日常查看和操作不放在这里。", "配置入口", "ok");
    Esp32BaseWeb::sendChunk("<div class='section-title'><h2>系统配置</h2></div><div class='menu-grid'><a class='menu-row' href='/esp32base/app-config'><span class='icon-box'>参</span><span><span class='event-title'>系统设置</span><span class='event-detail'>RS485 通讯、温湿度记录、自动运行和主控板 IO</span></span></a><a class='menu-row' href='/settings#devices'><span class='icon-box'>设</span><span><span class='event-title'>设备管理</span><span class='event-detail'>按类型查看设备、绑定地址和启用状态</span></span></a><a class='menu-row' href='/settings#scan'><span class='icon-box'>扫</span><span><span class='event-title'>扫描 RS485 地址</span><span class='event-detail'>默认扫描 1-127 地址，在线未绑定地址可用于绑定</span></span></a><a class='menu-row' href='/settings#notify'><span class='icon-box info'>知</span><span><span class='event-title'>通知规则</span><span class='event-detail'>只保存规则，真实发送通道后续接入</span></span></a></div>");
    Esp32BaseWeb::sendChunk("<div class='section-title'><h2>温湿度设置</h2><span class='tag info'>当前 ");
    sendEsc(envText);
    Esp32BaseWeb::sendChunk("</span></div><div class='card'><form method='post' action='/api/config/env' data-success='温湿度设置已保存' data-reload='1' onsubmit='return faPost(this)'><div class='fieldgrid'>");
    sendBoolSelect("启用 SHT30", "enabled", envEnabled);
    sendNumberField("I2C 地址", "address", envAddress, 8, 119);
    sendNumberField("采样间隔 ms", "intervalMs", envInterval, 1000, 600000, 1000);
    sendNumberField("记录间隔 s", "recordIntervalS", envRecordInterval, 10, 86400, 10);
    Esp32BaseWeb::sendChunk("</div><p class='event-detail'>SDA/SCL 为当前主控板固定接线；常见 SHT30 地址 68 即 0x44。</p><div class='actions' style='margin-top:12px'><input type='submit' value='保存温湿度设置'></div></form><form method='post' action='/api/env/read-now' data-success='已触发立即读取' data-reload='1' onsubmit='return faPost(this)' style='margin-top:8px'><input class='secondary' type='submit' value='立即读取 SHT30'></form></div>");
    Esp32BaseWeb::sendChunk("<div class='section-title'><h2>下料器校准</h2></div><div class='card'><form method='post' action='/api/config/feed' data-success='下料参数已保存' data-reload='1' onsubmit='return faPost(this)'><div class='fieldgrid'>");
    sendNumberField("分站地址", "stationAddress", feedConfig.station_address, 1, 127);
    sendNumberField("每圈脉冲", "pulsesPerTurn", static_cast<int32_t>(feedConfig.pulses_per_turn), 1, 200000);
    sendNumberField("每圈毫克", "gramsPerTurnMg", static_cast<int32_t>(feedConfig.grams_per_turn_mg), 1, 1000000);
    sendDirectionSelect("下料方向", "direction", feedConfig.feed_direction);
    sendNumberField("速度", "speedPermille", feedConfig.speed_permille, 1, 1000);
    sendNumberField("过流 mA", "overCurrentMa", feedConfig.over_current_ma, 1, 10000);
    sendNumberField("最长运行 ms", "maxRunMs", static_cast<int32_t>(feedConfig.max_run_ms), 100, 600000, 100);
    sendNumberField("最大脉冲", "maxActionPulses", static_cast<int32_t>(feedConfig.max_action_pulses), 1, 2000000);
    Esp32BaseWeb::sendChunk("</div><div class='actions' style='margin-top:12px'><input type='submit' value='保存下料参数'></div></form></div>");
    Esp32BaseWeb::sendChunk("<div class='section-title'><h2>门控校准</h2></div><div class='card'><form method='post' action='/api/config/door' data-success='门控参数已保存' data-reload='1' onsubmit='return faPost(this)'><div class='fieldgrid'>");
    sendNumberField("分站地址", "stationAddress", doorConfig.station_address, 1, 127);
    sendNumberField("每圈脉冲", "pulsesPerTurn", static_cast<int32_t>(doorConfig.pulses_per_turn), 1, 200000);
    sendNumberField("行程脉冲", "travelPulses", static_cast<int32_t>(doorConfig.travel_pulses), 1, 2000000);
    sendDirectionSelect("开门方向", "openDirection", doorConfig.open_direction);
    sendDirectionSelect("关门方向", "closeDirection", doorConfig.close_direction);
    sendNumberField("速度", "speedPermille", doorConfig.speed_permille, 1, 1000);
    sendNumberField("过流 mA", "overCurrentMa", doorConfig.over_current_ma, 1, 10000);
    sendNumberField("最长运行 ms", "maxRunMs", static_cast<int32_t>(doorConfig.max_run_ms), 100, 600000, 100);
    sendNumberField("最大脉冲", "maxActionPulses", static_cast<int32_t>(doorConfig.max_action_pulses), 1, 2000000);
    Esp32BaseWeb::sendChunk("</div><div class='actions' style='margin-top:12px'><input type='submit' value='保存门控参数'></div></form></div>");
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
            Esp32BaseWeb::sendChunk("</strong></div><div class='form-grid' style='margin-top:12px'>");
            Esp32BaseWeb::sendChunk("<form method='post' action='/api/devices/name' data-success='设备名称已保存' data-reload='1' onsubmit='return faPost(this)'><input type='hidden' name='deviceId' value='");
            sendU32(device.device_id);
            Esp32BaseWeb::sendChunk("'><div class='field'><label>设备名称</label><input name='name' maxlength='23' value='");
            sendEsc(device.name);
            Esp32BaseWeb::sendChunk("'></div><div class='actions'><input class='secondary' type='submit' value='保存名称'></div></form>");
            Esp32BaseWeb::sendChunk("<form method='post' action='/api/devices/display-order' data-success='显示顺序已保存' data-reload='1' onsubmit='return faPost(this)'><input type='hidden' name='deviceId' value='");
            sendU32(device.device_id);
            Esp32BaseWeb::sendChunk("'><div class='fieldgrid'>");
            sendNumberField("显示编号", "displayNo", device.display_no, 1, 9999);
            sendNumberField("排序", "sortOrder", device.sort_order, 0, 65535);
            Esp32BaseWeb::sendChunk("</div><div class='actions'><input class='secondary' type='submit' value='保存排序'></div></form>");
            Esp32BaseWeb::sendChunk("<div class='button-row'><form method='post' action='/api/devices/enabled' data-confirm-title='切换设备启用状态' data-confirm-note='停用后该业务设备不会参与计划和手动动作。' data-success='设备启用状态已保存' data-reload='1' onsubmit='return faPost(this)'><input type='hidden' name='deviceId' value='");
            sendU32(device.device_id);
            Esp32BaseWeb::sendChunk("'><input type='hidden' name='enabled' value='");
            Esp32BaseWeb::sendChunk(device.enabled != 0u ? "0" : "1");
            Esp32BaseWeb::sendChunk("'><input");
            if (device.enabled != 0u) {
                Esp32BaseWeb::sendChunk(" class='danger'");
            }
            Esp32BaseWeb::sendChunk(" type='submit' value='");
            Esp32BaseWeb::sendChunk(device.enabled != 0u ? "停用设备" : "启用设备");
            Esp32BaseWeb::sendChunk("'></form><form method='post' action='/api/devices/bind-station' data-confirm-title='绑定 RS485 分站' data-confirm-note='绑定后该业务设备会使用新的分站地址执行动作。' data-success='设备绑定已保存' data-reload='1' onsubmit='return faPost(this)'><input type='hidden' name='deviceId' value='");
            sendU32(device.device_id);
            Esp32BaseWeb::sendChunk("'><div class='field'><label>绑定地址</label><input type='number' name='address' min='1' max='127' value='");
            if (hasStation) {
                sendU32(station.bus_address);
            } else {
                Esp32BaseWeb::sendChunk("1");
            }
            Esp32BaseWeb::sendChunk("'></div><input class='secondary' type='submit' value='绑定地址'></form></div></div></div>");
        }
    } else {
        Esp32BaseWeb::sendChunk("<div class='card'><h2>设备表不可用</h2><p class='muted'>LittleFS 设备表未就绪。</p></div>");
    }
    Esp32BaseWeb::sendChunk("</div><div class='section-title' id='scan'><h2>扫描 RS485 地址</h2></div><div class='card'><form method='post' action='/api/bus/scan' data-success='RS485 扫描已完成，分站表已刷新' onsubmit='return faPost(this)'><div class='fieldgrid'><div class='field'><label>起始地址</label><input type='number' name='start' min='1' max='127' value='1'></div><div class='field'><label>结束地址</label><input type='number' name='end' min='1' max='127' value='127'></div><div class='field'><label>超时 ms</label><input type='number' name='timeout' min='20' max='2000' value='25'></div></div><div class='actions' style='margin-top:12px'><input type='submit' value='扫描 RS485 地址'></div></form></div>");
    Esp32BaseWeb::sendChunk("<div class='section-title' id='notify'><h2>通知规则</h2><span class='tag warn'>仅保存规则</span></div><div class='card'><form method='post' action='/api/config/notify' data-success='通知规则已保存' data-reload='1' onsubmit='return faPost(this)'><div class='fieldgrid'>");
    sendBoolSelect("总开关", "enabled", notifyEnabled);
    sendBoolSelect("动作完成", "actionDone", notifyActionDone);
    sendBoolSelect("动作失败", "actionFailed", notifyActionFailed);
    sendBoolSelect("分站故障", "stationFault", notifyStationFault);
    sendBoolSelect("分站离线", "stationOffline", notifyStationOffline);
    sendBoolSelect("计划跳过", "scheduleSkipped", notifyScheduleSkipped);
    sendBoolSelect("上电恢复", "powerRestored", notifyPowerRestored);
    Esp32BaseWeb::sendChunk("</div><p class='event-detail'>当前固件只保存规则；巴法云或微信真实发送属于后续专项。</p><div class='actions' style='margin-top:12px'><input type='submit' value='保存通知规则'></div></form></div>");
    sendV3Footer(V3Page::Settings);
}
