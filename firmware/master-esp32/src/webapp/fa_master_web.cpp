#include "fa_master_web_internal.h"

#include <stdlib.h>
#include <string.h>

#include "fa_auto_scheduler.h"
#include "fa_notification_rules.h"

FaFeedService* g_feed_service = nullptr;
FaDoorService* g_door_service = nullptr;
FaDeviceRegistry* g_device_registry = nullptr;
FaRs485Master* g_rs485_master = nullptr;
FaRs485Transport* g_transport = nullptr;
FaMasterActionRuntime* g_action_runtime = nullptr;
FaAutoScheduler* g_auto_scheduler = nullptr;
FaEnvSensorService* g_env_sensor = nullptr;
FaBoardIoService* g_board_io = nullptr;

namespace {

bool registerRouteChecked(const char* path, Esp32BaseWeb::Method method, Esp32BaseWeb::Handler handler) {
    const bool ok = Esp32BaseWeb::addRoute(path, method, handler);
    if (!ok) {
        ESP32BASE_LOG_E("farm", "web_route_register_failed path=%s", path != nullptr ? path : "-");
    }
    return ok;
}

bool registerPageChecked(const char* path, const char* title, Esp32BaseWeb::Handler handler) {
    const bool ok = Esp32BaseWeb::addPage(path, title, handler);
    if (!ok) {
        ESP32BASE_LOG_E("farm", "web_page_register_failed path=%s", path != nullptr ? path : "-");
    }
    return ok;
}

bool registerApiChecked(const char* path, Esp32BaseWeb::Handler handler) {
    const bool ok = Esp32BaseWeb::addApi(path, handler);
    if (!ok) {
        ESP32BASE_LOG_E("farm", "web_api_register_failed path=%s", path != nullptr ? path : "-");
    }
    return ok;
}

}  // namespace

uint32_t readUIntParam(const char* name, uint32_t fallback) {
    char raw[16] = "";
    if (!Esp32BaseWeb::getParam(name, raw, sizeof(raw)) || raw[0] == '\0') {
        return fallback;
    }
    char* end = nullptr;
    const unsigned long value = strtoul(raw, &end, 10);
    if (end == raw || *end != '\0') {
        return fallback;
    }
    return static_cast<uint32_t>(value);
}

const char* statusName(uint8_t status) {
    switch (status) {
    case FA_STATUS_OK:
        return "ok";
    case FA_STATUS_ERR_BAD_PARAM:
        return "bad_param";
    case FA_STATUS_ERR_NOT_CONFIGURED:
        return "not_configured";
    case FA_STATUS_ERR_BUSY:
        return "busy";
    case FA_STATUS_ERR_FAULT_ACTIVE:
        return "fault_active";
    case FA_STATUS_ERR_ACTION_DUPLICATE:
        return "action_duplicate";
    case FA_STATUS_ERR_SAFETY_BLOCKED:
        return "safety_blocked";
    case FA_STATUS_ERR_BAD_LENGTH:
        return "bad_length";
    case FA_STATUS_ERR_UNSUPPORTED_VERSION:
        return "unsupported_version";
    case FA_STATUS_ERR_ADDRESS_RESERVED:
        return "address_reserved";
    case FA_STATUS_ERR_INTERNAL:
        return "internal";
    default:
        return "error";
    }
}

void sendNumber(uint32_t value) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%lu", static_cast<unsigned long>(value));
    Esp32BaseWeb::sendChunk(buf);
}

const char* uiEnabled(bool value) {
    return value ? "启用" : "停用";
}

const char* uiReady(bool value) {
    return value ? "就绪" : "未就绪";
}

const char* uiActionState(bool busy) {
    return busy ? "执行中" : "空闲";
}

const char* uiTransportMode(uint8_t mode) {
    switch (mode) {
    case FA_RS485_MODE_DISABLED:
        return "停用";
    case FA_RS485_MODE_REAL_UART:
        return "真实串口";
    case FA_RS485_MODE_SIMULATED:
        return "模拟分站";
    default:
        return "未知";
    }
}

void formatDeviceLabel(uint16_t device_id, char* out, size_t len) {
    if (out == nullptr || len == 0u) {
        return;
    }
    FaDeviceRecord device;
    if (g_device_registry != nullptr &&
        g_device_registry->isReady() &&
        g_device_registry->deviceById(device_id, device)) {
        snprintf(out, len, "%s (#%u)", device.name, device.device_id);
        return;
    }
    snprintf(out, len, "#%u", device_id);
}

void copyActionRecordDeviceName(FaActionRecordStart& start, const FaWebDeviceStatus& status) {
    memset(start.device_name, 0, sizeof(start.device_name));
    if (status.device_name[0] != '\0') {
        strncpy(start.device_name, status.device_name, sizeof(start.device_name) - 1u);
        return;
    }
    FaDeviceRecord device;
    if (g_device_registry != nullptr &&
        g_device_registry->isReady() &&
        g_device_registry->deviceById(status.device_id, device)) {
        strncpy(start.device_name, device.name, sizeof(start.device_name) - 1u);
    }
}

const char* stationOnlineStateName(uint8_t state) {
    switch (state) {
    case FA_STATION_ONLINE_UNKNOWN:
        return "unknown";
    case FA_STATION_ONLINE_ONLINE:
        return "online";
    case FA_STATION_ONLINE_OFFLINE:
        return "offline";
    case FA_STATION_ONLINE_ERROR:
        return "error";
    case FA_STATION_ONLINE_CONFLICT_SUSPECTED:
        return "conflict_suspected";
    case FA_STATION_ONLINE_RESERVED_ADDRESS:
        return "reserved_address";
    default:
        return "unknown";
    }
}

const char* uiStationOnlineState(uint8_t state) {
    switch (state) {
    case FA_STATION_ONLINE_UNKNOWN:
        return "未知";
    case FA_STATION_ONLINE_ONLINE:
        return "在线";
    case FA_STATION_ONLINE_OFFLINE:
        return "离线";
    case FA_STATION_ONLINE_ERROR:
        return "异常";
    case FA_STATION_ONLINE_CONFLICT_SUSPECTED:
        return "疑似地址冲突";
    case FA_STATION_ONLINE_RESERVED_ADDRESS:
        return "保留地址";
    default:
        return "未知";
    }
}

bool readDeviceStatus(uint8_t device_type,
                      uint16_t fallback_device_id,
                      uint8_t fallback_station_address,
                      FaWebDeviceStatus& out) {
    out = {};
    out.device_id = fallback_device_id;
    out.station_address = fallback_station_address;
    out.device_enabled = true;
    out.station_online_state = FA_STATION_ONLINE_UNKNOWN;

    if (g_device_registry == nullptr || !g_device_registry->isReady()) {
        return true;
    }
    out.registry_ready = true;

    FaDeviceRecord device;
    if (!g_device_registry->deviceByType(device_type, device)) {
        return true;
    }
    out.has_device = true;
    out.device_id = device.device_id;
    out.device_enabled = device.enabled != 0u;
    strncpy(out.device_name, device.name, sizeof(out.device_name) - 1u);

    FaStationRecord station;
    if (g_device_registry->stationById(device.station_id, station)) {
        out.has_station = true;
        out.station_online_state = station.online_state;
        out.last_seen_at = station.last_seen_at;
        out.last_error = station.last_error;
        if (fa_address_is_normal(station.bus_address)) {
            out.station_address = station.bus_address;
        }
    }
    return out.device_enabled;
}

bool deviceStatusBlocksStart(const FaWebDeviceStatus& status) {
    if (!status.device_enabled) {
        return true;
    }
    if (!status.has_station) {
        return false;
    }
    return status.station_online_state != FA_STATION_ONLINE_UNKNOWN &&
           status.station_online_state != FA_STATION_ONLINE_ONLINE;
}

void formatStationStatusLabel(const FaWebDeviceStatus& status, char* out, size_t len) {
    if (out == nullptr || len == 0u) {
        return;
    }
    if (!status.registry_ready) {
        snprintf(out, len, "设备表不可用");
        return;
    }
    if (!status.has_device) {
        snprintf(out, len, "使用配置默认值");
        return;
    }
    if (!status.has_station) {
        snprintf(out, len, "未绑定分站");
        return;
    }
    if (status.last_error != 0u) {
        snprintf(out, len, "%s，错误 %u", uiStationOnlineState(status.station_online_state), status.last_error);
        return;
    }
    snprintf(out, len, "%s", uiStationOnlineState(status.station_online_state));
}

void sendDeviceStatusBlockedJson(const FaWebDeviceStatus& status) {
    ESP32BASE_LOG_W("farm", "action_blocked station_not_ready device_id=%u addr=%u state=%s last_error=%u",
                    status.device_id,
                    status.station_address,
                    stationOnlineStateName(status.station_online_state),
                    status.last_error);
    Esp32BaseWeb::beginJson(409);
    Esp32BaseWeb::sendChunk("\"ok\":false,\"error\":\"station_not_ready\",\"stationAddress\":");
    sendNumber(status.station_address);
    Esp32BaseWeb::sendChunk(",\"stationState\":\"");
    Esp32BaseWeb::sendChunk(stationOnlineStateName(status.station_online_state));
    Esp32BaseWeb::sendChunk("\",\"lastError\":");
    sendNumber(status.last_error);
    Esp32BaseWeb::sendChunk(",\"message\":\"station state blocks starting a new action\"");
    Esp32BaseWeb::endJson();
}

const char* frameResultName(FaFrameResult result) {
    switch (result) {
    case FA_FRAME_OK:
        return "ok";
    case FA_FRAME_ERR_NULL:
        return "null";
    case FA_FRAME_ERR_TOO_SHORT:
        return "too_short";
    case FA_FRAME_ERR_BAD_SOF:
        return "bad_sof";
    case FA_FRAME_ERR_UNSUPPORTED_VERSION:
        return "unsupported_version";
    case FA_FRAME_ERR_PAYLOAD_TOO_LONG:
        return "payload_too_long";
    case FA_FRAME_ERR_LENGTH_MISMATCH:
        return "length_mismatch";
    case FA_FRAME_ERR_CRC:
        return "crc";
    case FA_FRAME_ERR_OUTPUT_TOO_SMALL:
        return "output_too_small";
    default:
        return "frame_error";
    }
}

const char* recordStateName(uint8_t state) {
    switch (state) {
    case FA_ACTION_RECORD_RUNNING:
        return "running";
    case FA_ACTION_RECORD_COMPLETED:
        return "completed";
    case FA_ACTION_RECORD_STOPPED:
        return "stopped";
    case FA_ACTION_RECORD_FAILED:
        return "failed";
    default:
        return "unknown";
    }
}

const char* uiRecordState(uint8_t state) {
    switch (state) {
    case FA_ACTION_RECORD_RUNNING:
        return "执行中";
    case FA_ACTION_RECORD_COMPLETED:
        return "已完成";
    case FA_ACTION_RECORD_STOPPED:
        return "已停止";
    case FA_ACTION_RECORD_FAILED:
        return "失败";
    default:
        return "未知";
    }
}

const char* stopReasonName(uint8_t reason) {
    switch (reason) {
    case FA_STOP_NONE:
        return "none";
    case FA_STOP_TARGET_REACHED:
        return "target";
    case FA_STOP_MASTER_COMMAND:
        return "master_stop";
    case FA_STOP_OVER_CURRENT:
        return "over_current";
    case FA_STOP_STALL:
        return "stall";
    case FA_STOP_TIMEOUT:
        return "timeout";
    case FA_STOP_TARGET_OVERRUN:
        return "overrun";
    case FA_STOP_WATCHDOG:
        return "watchdog";
    case FA_STOP_LOCAL_FAULT:
        return "local_fault";
    default:
        return "unknown";
    }
}

const char* uiStopReason(uint8_t reason) {
    switch (reason) {
    case FA_STOP_NONE:
        return "无";
    case FA_STOP_TARGET_REACHED:
        return "到达目标";
    case FA_STOP_MASTER_COMMAND:
        return "主控停止";
    case FA_STOP_OVER_CURRENT:
        return "过流";
    case FA_STOP_STALL:
        return "堵转";
    case FA_STOP_TIMEOUT:
        return "超时";
    case FA_STOP_TARGET_OVERRUN:
        return "超过目标";
    case FA_STOP_WATCHDOG:
        return "看门狗";
    case FA_STOP_LOCAL_FAULT:
        return "本地故障";
    default:
        return "未知";
    }
}

const char* faultName(uint16_t fault) {
    switch (fault) {
    case FA_FAULT_NONE:
        return "none";
    case FA_FAULT_OVER_CURRENT:
        return "over_current";
    case FA_FAULT_STALL:
        return "stall";
    case FA_FAULT_ENCODER_LOST:
        return "encoder_lost";
    case FA_FAULT_RUN_TIMEOUT:
        return "run_timeout";
    case FA_FAULT_TARGET_OVERRUN:
        return "target_overrun";
    case FA_FAULT_CONFIG_INVALID:
        return "config_invalid";
    case FA_FAULT_DRIVER_ABNORMAL:
        return "driver_abnormal";
    case FA_FAULT_CURRENT_SENSOR:
        return "current_sensor";
    case FA_FAULT_WATCHDOG_RESET:
        return "watchdog_reset";
    case FA_FAULT_RESERVED_ADDRESS:
        return "reserved_address";
    case FA_FAULT_COMMAND_REJECTED:
        return "command_rejected";
    case FA_FAULT_COMMUNICATION:
        return "communication";
    default:
        return "unknown";
    }
}

const char* uiFaultName(uint16_t fault) {
    switch (fault) {
    case FA_FAULT_NONE:
        return "无";
    case FA_FAULT_OVER_CURRENT:
        return "过流";
    case FA_FAULT_STALL:
        return "堵转";
    case FA_FAULT_ENCODER_LOST:
        return "编码器丢失";
    case FA_FAULT_RUN_TIMEOUT:
        return "运行超时";
    case FA_FAULT_TARGET_OVERRUN:
        return "超过目标";
    case FA_FAULT_CONFIG_INVALID:
        return "配置无效";
    case FA_FAULT_DRIVER_ABNORMAL:
        return "驱动异常";
    case FA_FAULT_CURRENT_SENSOR:
        return "电流采样异常";
    case FA_FAULT_WATCHDOG_RESET:
        return "看门狗复位";
    case FA_FAULT_RESERVED_ADDRESS:
        return "保留地址";
    case FA_FAULT_COMMAND_REJECTED:
        return "命令拒绝";
    case FA_FAULT_COMMUNICATION:
        return "通讯异常";
    default:
        return "未知";
    }
}

const char* uiRuntimeError(const char* error) {
    if (error == nullptr || error[0] == '\0' || strcmp(error, "none") == 0 || strcmp(error, "ok") == 0) {
        return "无";
    }
    if (strcmp(error, "busy") == 0) {
        return "忙";
    }
    if (strcmp(error, "bad_start") == 0) {
        return "动作启动参数异常";
    }
    if (strcmp(error, "transport_unavailable") == 0 || strcmp(error, "not_configured") == 0) {
        return "通讯未配置";
    }
    if (strcmp(error, "build_status") == 0) {
        return "状态请求构建失败";
    }
    if (strcmp(error, "bad_status") == 0) {
        return "分站状态异常";
    }
    if (strcmp(error, "record_status") == 0) {
        return "记录状态失败";
    }
    if (strcmp(error, "record_append") == 0) {
        return "记录写入失败";
    }
    if (strcmp(error, "timeout") == 0) {
        return "通讯超时";
    }
    if (strcmp(error, "bad_frame") == 0) {
        return "通讯帧异常";
    }
    return "未知错误";
}

void formatDurationMs(uint32_t ms, char* out, size_t len) {
    if (out == nullptr || len == 0u) {
        return;
    }
    if (ms >= 1000u) {
        snprintf(out, len, "%lu.%03lus", static_cast<unsigned long>(ms / 1000u), static_cast<unsigned long>(ms % 1000u));
    } else {
        snprintf(out, len, "%lums", static_cast<unsigned long>(ms));
    }
}

void formatTimeValue(uint32_t seconds, char* out, size_t len) {
    if (out == nullptr || len == 0u) {
        return;
    }
    if (seconds == 0u) {
        snprintf(out, len, "-");
        return;
    }
    if (!Esp32BaseTime::formatEpoch(seconds, out, len, "%m-%d %H:%M:%S")) {
        snprintf(out, len, "%lus", static_cast<unsigned long>(seconds));
    }
}

void formatAmount(const FaActionRecord& record, char* out, size_t len) {
    if (out == nullptr || len == 0u) {
        return;
    }
    if (record.amount_mode == FA_FEED_AMOUNT_MG) {
        snprintf(out, len, "%lu mg", static_cast<unsigned long>(record.amount_value));
    } else if (record.amount_mode == FA_FEED_AMOUNT_TURNS_X1000) {
        snprintf(out, len, "%lu/1000 圈", static_cast<unsigned long>(record.amount_value));
    } else if (record.amount_mode == FA_ACTION_RECORD_AMOUNT_PULSES) {
        snprintf(out, len, "%lu 脉冲", static_cast<unsigned long>(record.amount_value));
    } else {
        snprintf(out, len, "%lu", static_cast<unsigned long>(record.amount_value));
    }
}

void sendRecordTableRow(const FaActionRecord& record) {
    char started[24];
    char duration[20];
    char amount[28];
    char device[36];
    formatTimeValue(record.started_at_s, started, sizeof(started));
    formatDurationMs(record.run_ms, duration, sizeof(duration));
    formatAmount(record, amount, sizeof(amount));
    if (record.device_name[0] != '\0') {
        snprintf(device, sizeof(device), "%s (#%u)", record.device_name, record.device_id);
    } else {
        formatDeviceLabel(record.device_id, device, sizeof(device));
    }

    Esp32BaseWeb::sendChunk("<tr><td>");
    sendNumber(record.action_id);
    Esp32BaseWeb::sendChunk("</td><td>");
    Esp32BaseWeb::writeHtmlEscaped(device);
    Esp32BaseWeb::sendChunk("</td><td>");
    Esp32BaseWeb::writeHtmlEscaped(uiRecordState(record.state));
    Esp32BaseWeb::sendChunk("</td><td>");
    Esp32BaseWeb::writeHtmlEscaped(started);
    Esp32BaseWeb::sendChunk("</td><td>");
    Esp32BaseWeb::writeHtmlEscaped(amount);
    Esp32BaseWeb::sendChunk("</td><td>");
    sendNumber(record.completed_pulses);
    Esp32BaseWeb::sendChunk(" / ");
    sendNumber(record.target_pulses);
    Esp32BaseWeb::sendChunk("</td><td>");
    Esp32BaseWeb::writeHtmlEscaped(duration);
    Esp32BaseWeb::sendChunk("</td><td>");
    sendNumber(record.bus_address);
    Esp32BaseWeb::sendChunk("</td><td>");
    Esp32BaseWeb::writeHtmlEscaped(uiStopReason(record.stop_reason));
    Esp32BaseWeb::sendChunk("</td><td>");
    Esp32BaseWeb::writeHtmlEscaped(uiFaultName(record.fault_code));
    Esp32BaseWeb::sendChunk("</td></tr>");
}

void sendActiveActionPanel(void) {
    if (g_action_runtime == nullptr || !g_action_runtime->isBusy()) {
        return;
    }

    const FaActionRecord* active = g_action_runtime->activeRecord();
    if (active == nullptr) {
        return;
    }

    char amount[28];
    char device[36];
    formatAmount(*active, amount, sizeof(amount));
    if (active->device_name[0] != '\0') {
        snprintf(device, sizeof(device), "%s (#%u)", active->device_name, active->device_id);
    } else {
        formatDeviceLabel(active->device_id, device, sizeof(device));
    }
    Esp32BaseWeb::beginPanel("当前动作");
    Esp32BaseWeb::sendInfoRowCompact("动作", "主控正在轮询跟踪的动作。", uiRecordState(active->state));
    Esp32BaseWeb::sendInfoRowCompact("设备", "发起该动作的业务设备。", device);
    Esp32BaseWeb::sendInfoRowCompact("数量", "原始手动请求。", amount);
    char progress[36];
    snprintf(progress, sizeof(progress), "%lu / %lu 脉冲",
             static_cast<unsigned long>(active->completed_pulses),
             static_cast<unsigned long>(active->target_pulses));
    Esp32BaseWeb::sendInfoRowCompact("进度", "分站最近一次回报的状态。", progress);
    char run[20];
    char current[28];
    formatDurationMs(active->run_ms, run, sizeof(run));
    snprintf(current, sizeof(current), "%u / %u mA", active->current_ma, active->peak_current_ma);
    Esp32BaseWeb::sendInfoRowCompact("运行", "分站回报的运行时间。", run);
    Esp32BaseWeb::sendInfoRowCompact("电流", "当前电流 / 峰值电流。", current);
    Esp32BaseWeb::sendInfoRowCompact("停止原因", "分站最近一次停止原因。", uiStopReason(active->stop_reason));
    Esp32BaseWeb::sendInfoRowCompact("故障", "分站最近一次故障码。", uiFaultName(active->fault_code));
    Esp32BaseWeb::sendInfoRowCompact("最近错误", "主控侧最近一次轮询错误。", uiRuntimeError(g_action_runtime->lastError()));
    Esp32BaseWeb::sendChunk("<div class='actions'><form method='post' action='/api/action/stop-active' onsubmit='return once(this)'><input class='danger' type='submit' value='停止当前动作'></form></div>");
    Esp32BaseWeb::endPanel();
}

void sendRecentRecordsPanel(void) {
    Esp32BaseWeb::beginPanel("最近记录");
    if (!FaActionRecordStore::isReady()) {
        Esp32BaseWeb::sendNotice(Esp32BaseWeb::UI_WARN, "记录不可用", "LittleFS 动作记录存储未就绪。");
        Esp32BaseWeb::endPanel();
        return;
    }
    const uint16_t count = FaActionRecordStore::count();
    if (count == 0u) {
        Esp32BaseWeb::sendInfoRowCompact("暂无记录", "完成或失败的动作会显示在这里。");
        Esp32BaseWeb::endPanel();
        return;
    }

    const uint16_t limit = count < kRecentRecordLimit ? count : kRecentRecordLimit;
    Esp32BaseWeb::sendChunk("<div class='tablewrap'><table class='evtable'><thead><tr><th>ID</th><th>设备</th><th>状态</th><th>开始时间</th><th>数量</th><th>脉冲</th><th>运行</th><th>地址</th><th>停止</th><th>故障</th></tr></thead><tbody>");
    for (uint16_t i = 0u; i < limit; ++i) {
        FaActionRecord record;
        if (FaActionRecordStore::readLatest(i, record)) {
            sendRecordTableRow(record);
        }
    }
    Esp32BaseWeb::sendChunk("</tbody></table></div>");
    Esp32BaseWeb::endPanel();
}

void sendFeedTransportError(uint16_t http_code, const char* stage, const char* error_key, const char* error_value) {
    ESP32BASE_LOG_W("farm", "transport_error stage=%s %s=%s http=%u",
                    stage != nullptr ? stage : "-",
                    error_key != nullptr ? error_key : "error",
                    error_value != nullptr ? error_value : "-",
                    http_code);
    Esp32BaseWeb::beginJson(http_code);
    Esp32BaseWeb::sendChunk("\"ok\":false,\"stage\":\"");
    Esp32BaseWeb::sendChunk(stage);
    Esp32BaseWeb::sendChunk("\",\"");
    Esp32BaseWeb::sendChunk(error_key);
    Esp32BaseWeb::sendChunk("\":\"");
    Esp32BaseWeb::sendChunk(error_value);
    Esp32BaseWeb::sendChunk("\"");
    Esp32BaseWeb::endJson();
}

bool transactAndParseCommon(uint8_t station_address,
                            uint8_t expected_seq,
                            uint8_t expected_cmd,
                            const uint8_t* request,
                            size_t request_len,
                            FaMasterCommonResponse* common,
                            const char* stage) {
    if (common != nullptr) {
        memset(common, 0, sizeof(*common));
    }
    uint8_t response[FA_MAX_FRAME_LEN];
    size_t response_len = 0u;
    const FaRs485TransportStatus tx_status = g_transport->transact(request, request_len, response, sizeof(response), &response_len, 0u);
    if (tx_status != FaRs485TransportStatus::OK) {
        sendFeedTransportError(tx_status == FaRs485TransportStatus::TIMEOUT ? 504 : 502,
                               stage,
                               "transport",
                               FaRs485Transport::statusName(tx_status));
        return false;
    }

    const uint8_t status = fa_rs485_master_parse_common(response, response_len, station_address, expected_seq, expected_cmd, common);
    if (status != FA_STATUS_OK) {
        Esp32BaseWeb::beginJson(502);
        Esp32BaseWeb::sendChunk("\"ok\":false,\"stage\":\"");
        Esp32BaseWeb::sendChunk(stage);
        Esp32BaseWeb::sendChunk("\",\"status\":\"");
        Esp32BaseWeb::sendChunk(statusName(status));
        Esp32BaseWeb::sendChunk("\",\"faultCode\":");
        sendNumber(common != nullptr ? common->fault_code : 0u);
        Esp32BaseWeb::endJson();
        return false;
    }
    return true;
}

void sendStopActiveActionApi(void) {
    if (!Esp32BaseWeb::checkPostAllowed("action_stop")) {
        return;
    }
    if (g_rs485_master == nullptr || g_transport == nullptr || g_action_runtime == nullptr) {
        Esp32BaseWeb::sendJson(503, "{\"ok\":false,\"error\":\"service_unavailable\"}");
        return;
    }
    if (!g_action_runtime->isBusy() || g_action_runtime->activeRecord() == nullptr) {
        Esp32BaseWeb::sendJson(409, "{\"ok\":false,\"error\":\"no_active_action\"}");
        return;
    }
    if (!g_transport->isReady()) {
        Esp32BaseWeb::sendJson(503, "{\"ok\":false,\"transport\":\"not_configured\"}");
        return;
    }

    const FaActionRecord* active = g_action_runtime->activeRecord();
    const uint8_t station_address = active->bus_address;
    uint8_t request[FA_MAX_FRAME_LEN];
    size_t request_len = 0u;
    uint8_t seq = 0u;
    const FaFrameResult frame_result = fa_rs485_master_build_stop_action(g_rs485_master,
                                                                        station_address,
                                                                        request,
                                                                        sizeof(request),
                                                                        &request_len,
                                                                        &seq);
    if (frame_result != FA_FRAME_OK) {
        sendFeedTransportError(500, "stop_active", "frame", frameResultName(frame_result));
        return;
    }
    FaMasterCommonResponse common;
    if (!transactAndParseCommon(station_address, seq, FA_CMD_STOP_ACTION, request, request_len, &common, "stop_active")) {
        return;
    }

    ESP32BASE_LOG_I("farm", "stop_active_accepted action_id=%lu addr=%u",
                    static_cast<unsigned long>(active->action_id),
                    station_address);
    Esp32BaseWeb::beginJson(200);
    Esp32BaseWeb::sendChunk("\"ok\":true,\"command\":\"stop_active\",\"actionId\":");
    sendNumber(active->action_id);
    Esp32BaseWeb::sendChunk(",\"stationAddress\":");
    sendNumber(station_address);
    Esp32BaseWeb::sendChunk(",\"message\":\"stop accepted by active station\"");
    Esp32BaseWeb::endJson();
}

void sendRecordsPage(void) {
    if (!Esp32BaseWeb::checkAuth()) {
        return;
    }

    char value[24];
    Esp32BaseWeb::sendHeader("记录");
    Esp32BaseWeb::sendPageTitle("动作记录", "查看最近的 FarmAuto 动作记录，以及当前正在跟踪的动作。");

    Esp32BaseWeb::beginMetricGrid();
    snprintf(value, sizeof(value), "%u", FaActionRecordStore::count());
    Esp32BaseWeb::sendMetric("记录数", value, FaActionRecordStore::isReady() ? "就绪" : "不可用");
    snprintf(value, sizeof(value), "%u", FaActionRecordStore::capacity());
    Esp32BaseWeb::sendMetric("容量", value);
    snprintf(value, sizeof(value), "%lu", static_cast<unsigned long>(FaActionRecordStore::sequence()));
    Esp32BaseWeb::sendMetric("序号", value);
    Esp32BaseWeb::sendMetric("动作", uiActionState(g_action_runtime != nullptr && g_action_runtime->isBusy()));
    Esp32BaseWeb::endMetricGrid();

    sendActiveActionPanel();
    sendRecentRecordsPanel();
    Esp32BaseWeb::sendFooter();
}

void fa_master_web_register_config(void) {
    Esp32BaseWeb::setDeviceName("FarmAuto");
    Esp32BaseWeb::setHomePath("/home");
    Esp32BaseWeb::setHomeMode(Esp32BaseWeb::HOME_COMBINED);
    Esp32BaseWeb::setSystemNavMode(Esp32BaseWeb::SYSTEM_NAV_SECTION);
    Esp32BaseWeb::setBuiltinLabel(Esp32BaseWeb::BUILTIN_HOME, "状态");
    Esp32BaseWeb::setBuiltinLabel(Esp32BaseWeb::BUILTIN_WIFI, "网络");
    Esp32BaseWeb::setBuiltinLabel(Esp32BaseWeb::BUILTIN_OTA, "固件升级");
    Esp32BaseWeb::setBuiltinLabel(Esp32BaseWeb::BUILTIN_LOGS, "系统日志");
    Esp32BaseWeb::setBuiltinLabel(Esp32BaseWeb::BUILTIN_APP_EVENTS, "应用事件");
    Esp32BaseWeb::setBuiltinLabel(Esp32BaseWeb::BUILTIN_TOOLS, "系统工具");
    Esp32BaseWeb::setBuiltinLabel(Esp32BaseWeb::BUILTIN_SYSTEM, "系统");
    Esp32BaseWeb::setBuiltinLabel(Esp32BaseWeb::BUILTIN_AUTH, "登录密码");
    Esp32BaseAppConfig::setTitle("FarmAuto 配置");
    Esp32BaseAppConfig::addGroup({"feeder", "下料"});
    Esp32BaseAppConfig::addGroup({"door", "门控"});
    Esp32BaseAppConfig::addGroup({"auto", "自动"});
    Esp32BaseAppConfig::addGroup({"env", "温湿度"});
    Esp32BaseAppConfig::addGroup({"board", "主控板"});
    Esp32BaseAppConfig::addGroup({"notify", "通知"});
    Esp32BaseAppConfig::addGroup({"rs485", "RS485"});
    Esp32BaseAppConfig::addInt({"feeder", kNs, kStationAddress, "分站地址", 1, 1, 127, 1, nullptr,
                                "RS485 地址范围 1..127。", false, nullptr});
    Esp32BaseAppConfig::addInt({"feeder", kNs, kPulsesPerTurn, "每圈脉冲", 4320, 1, 200000, 1, "脉冲",
                                "输出轴转一圈对应的编码器脉冲数。", false, nullptr});
    Esp32BaseAppConfig::addInt({"feeder", kNs, kGramsPerTurnMg, "每圈毫克", 8000, 1, 1000000, 1, "mg",
                                "按重量下料时使用的标定值。", false, nullptr});
    Esp32BaseAppConfig::addInt({"feeder", kNs, kDirection, "方向", 1, -1, 1, 1, nullptr,
                                "1 为正转，-1 为反转。", false, nullptr});
    Esp32BaseAppConfig::addInt({"feeder", kNs, kSpeedPermille, "速度", 800, 1, 1000, 1, "千分比",
                                "下发给分站的电机速度请求。", false, nullptr});
    Esp32BaseAppConfig::addInt({"feeder", kNs, kOverCurrentMa, "过流阈值", 2000, 1, 10000, 1, "mA",
                                "分站本地保护阈值。", false, nullptr});
    Esp32BaseAppConfig::addInt({"feeder", kNs, kMaxRunMs, "最长运行", 60000, 100, 600000, 100, "ms",
                                "单次动作超时时间。", false, nullptr});
    Esp32BaseAppConfig::addInt({"feeder", kNs, kMaxActionPulses, "最大脉冲", 432000, 1, 2000000, 1, "脉冲",
                                "单次动作脉冲上限。", false, nullptr});
    Esp32BaseAppConfig::addInt({"door", kDoorNs, kDoorStationAddress, "分站地址", 2, 1, 127, 1, nullptr,
                                "RS485 地址范围 1..127。", false, nullptr});
    Esp32BaseAppConfig::addInt({"door", kDoorNs, kDoorPulsesPerTurn, "每圈脉冲", 4320, 1, 200000, 1, "脉冲",
                                "输出轴转一圈对应的编码器脉冲数。", false, nullptr});
    Esp32BaseAppConfig::addInt({"door", kDoorNs, kDoorTravelPulses, "门行程脉冲", 20000, 1, 2000000, 1, "脉冲",
                                "开门或关门一次有界动作的目标脉冲。", false, nullptr});
    Esp32BaseAppConfig::addInt({"door", kDoorNs, kDoorOpenDirection, "开门方向", 1, -1, 1, 1, nullptr,
                                "1 为正转，-1 为反转。", false, nullptr});
    Esp32BaseAppConfig::addInt({"door", kDoorNs, kDoorCloseDirection, "关门方向", -1, -1, 1, 1, nullptr,
                                "1 为正转，-1 为反转。", false, nullptr});
    Esp32BaseAppConfig::addInt({"door", kDoorNs, kDoorSpeedPermille, "速度", 700, 1, 1000, 1, "千分比",
                                "下发给分站的电机速度请求。", false, nullptr});
    Esp32BaseAppConfig::addInt({"door", kDoorNs, kDoorOverCurrentMa, "过流阈值", 2500, 1, 10000, 1, "mA",
                                "分站本地保护阈值。", false, nullptr});
    Esp32BaseAppConfig::addInt({"door", kDoorNs, kDoorMaxRunMs, "最长运行", 30000, 100, 600000, 100, "ms",
                                "单次动作超时时间。", false, nullptr});
    Esp32BaseAppConfig::addInt({"door", kDoorNs, kDoorMaxActionPulses, "最大脉冲", 100000, 1, 2000000, 1, "脉冲",
                                "单次动作脉冲上限。", false, nullptr});
    Esp32BaseAppConfig::addInt({"auto", FaAutoScheduleConfig::NS, FaAutoScheduleConfig::KEY_ENABLED, "自动启用", 1, 0, 1, 1, nullptr,
                                "1 表示启用每日自动计划。", false, nullptr});
    Esp32BaseAppConfig::addInt({"auto", FaAutoScheduleConfig::NS, FaAutoScheduleConfig::KEY_TZ_OFFSET_MIN, "时区偏移", 480, -720, 840, 1, "分钟",
                                "本地时间相对 UTC 的偏移，中国为 480。", false, nullptr});
    Esp32BaseAppConfig::addInt({"auto", FaAutoScheduleConfig::NS, FaAutoScheduleConfig::KEY_FEED_ENABLED, "下料计划", 1, 0, 1, 1, nullptr,
                                "1 表示启用每日下料时间点。", false, nullptr});
    Esp32BaseAppConfig::addInt({"auto", FaAutoScheduleConfig::NS, FaAutoScheduleConfig::KEY_FEED_1_MIN, "下料 1 分钟", 430, 0, 1439, 1, "分钟",
                                "当天分钟数，例如 430 表示 07:10。", false, nullptr});
    Esp32BaseAppConfig::addInt({"auto", FaAutoScheduleConfig::NS, FaAutoScheduleConfig::KEY_FEED_1_AMOUNT_MG, "下料 1 数量", 100000, 1, 5000000, 1, "mg",
                                "计划下料数量。", false, nullptr});
    Esp32BaseAppConfig::addInt({"auto", FaAutoScheduleConfig::NS, FaAutoScheduleConfig::KEY_FEED_2_MIN, "下料 2 分钟", 1090, 0, 1439, 1, "分钟",
                                "当天分钟数，例如 1090 表示 18:10。", false, nullptr});
    Esp32BaseAppConfig::addInt({"auto", FaAutoScheduleConfig::NS, FaAutoScheduleConfig::KEY_FEED_2_AMOUNT_MG, "下料 2 数量", 100000, 1, 5000000, 1, "mg",
                                "计划下料数量。", false, nullptr});
    Esp32BaseAppConfig::addInt({"auto", FaAutoScheduleConfig::NS, FaAutoScheduleConfig::KEY_DOOR_ENABLED, "门控计划", 1, 0, 1, 1, nullptr,
                                "1 表示启用每日开门/关门。", false, nullptr});
    Esp32BaseAppConfig::addInt({"auto", FaAutoScheduleConfig::NS, FaAutoScheduleConfig::KEY_DOOR_OPEN_MIN, "开门分钟", 480, 0, 1439, 1, "分钟",
                                "当天分钟数，例如 480 表示 08:00。", false, nullptr});
    Esp32BaseAppConfig::addInt({"auto", FaAutoScheduleConfig::NS, FaAutoScheduleConfig::KEY_DOOR_CLOSE_MIN, "关门分钟", 1050, 0, 1439, 1, "分钟",
                                "当天分钟数，例如 1050 表示 17:30。", false, nullptr});
    Esp32BaseAppConfig::addInt({"auto", FaAutoScheduleConfig::NS, FaAutoScheduleConfig::KEY_FEED_PAUSE_UNTIL, "下料暂停到", 0, 0, 2147483647, 1, "epoch",
                                "0 表示未暂停；epoch 秒表示暂停自动下料到该时间。", false, nullptr});
    Esp32BaseAppConfig::addInt({"auto", FaAutoScheduleConfig::NS, FaAutoScheduleConfig::KEY_DOOR_PAUSE_UNTIL, "门控暂停到", 0, 0, 2147483647, 1, "epoch",
                                "0 表示未暂停；epoch 秒表示暂停自动门控到该时间。", false, nullptr});
    Esp32BaseAppConfig::addBool({"env", FaEnvSensorConfig::NS, FaEnvSensorConfig::KEY_ENABLED, "启用 SHT30", true,
                                 "读取室外温湿度。", false, nullptr});
    Esp32BaseAppConfig::addInt({"env", FaEnvSensorConfig::NS, FaEnvSensorConfig::KEY_ADDRESS, "I2C 地址", 68, 8, 119, 1, nullptr,
                                "SHT30 7 位地址，常见 68 即 0x44。", false, nullptr});
    Esp32BaseAppConfig::addInt({"env", FaEnvSensorConfig::NS, FaEnvSensorConfig::KEY_INTERVAL_MS, "采样间隔", 5000, 1000, 600000, 1000, "ms",
                                "后台采样间隔。", false, nullptr});
    Esp32BaseAppConfig::addInt({"env", FaEnvSensorConfig::NS, FaEnvSensorConfig::KEY_RECORD_INTERVAL_S, "记录间隔", 300, 10, 86400, 10, "s",
                                "写入应用事件的最小间隔。", false, nullptr});
    Esp32BaseAppConfig::addBool({"board", FaBoardIoConfig::NS, FaBoardIoConfig::KEY_ENABLED, "启用主控板 IO", true,
                                 "驱动主控 RUN/ERR 灯，并读取本地按钮。", false, nullptr});
    Esp32BaseAppConfig::addInt({"board", FaBoardIoConfig::NS, FaBoardIoConfig::KEY_RUN_LED_PIN, "RUN 灯引脚", 27, -1, 39, 1, nullptr,
                                "-1 表示停用 RUN 灯输出。", false, nullptr});
    Esp32BaseAppConfig::addInt({"board", FaBoardIoConfig::NS, FaBoardIoConfig::KEY_ERR_LED_PIN, "ERR 灯引脚", 14, -1, 39, 1, nullptr,
                                "-1 表示停用 ERR 灯输出。", false, nullptr});
    Esp32BaseAppConfig::addBool({"board", FaBoardIoConfig::NS, FaBoardIoConfig::KEY_LED_ACTIVE_LOW, "LED 低电平点亮", false,
                                 "如果板载 LED 输出 LOW 时点亮，则启用。", false, nullptr});
    Esp32BaseAppConfig::addInt({"board", FaBoardIoConfig::NS, FaBoardIoConfig::KEY_BOOT_PIN, "BOOT 按钮引脚", 0, -1, 39, 1, nullptr,
                                "GPIO0 BOOT 按钮输入；-1 表示停用。", false, nullptr});
    Esp32BaseAppConfig::addInt({"board", FaBoardIoConfig::NS, FaBoardIoConfig::KEY_BUTTON_1_PIN, "按钮 1 引脚", -1, -1, 39, 1, nullptr,
                                "预留本地按钮；-1 表示停用。", false, nullptr});
    Esp32BaseAppConfig::addInt({"board", FaBoardIoConfig::NS, FaBoardIoConfig::KEY_BUTTON_2_PIN, "按钮 2 引脚", -1, -1, 39, 1, nullptr,
                                "开门按钮输入；动作绑定在共享门控执行器完成后接入。", false, nullptr});
    Esp32BaseAppConfig::addInt({"board", FaBoardIoConfig::NS, FaBoardIoConfig::KEY_BUTTON_3_PIN, "按钮 3 引脚", -1, -1, 39, 1, nullptr,
                                "关门按钮输入；动作绑定在共享门控执行器完成后接入。", false, nullptr});
    Esp32BaseAppConfig::addInt({"board", FaBoardIoConfig::NS, FaBoardIoConfig::KEY_BUTTON_4_PIN, "按钮 4 引脚", -1, -1, 39, 1, nullptr,
                                "停止按钮输入；动作绑定在共享门控执行器完成后接入。", false, nullptr});
    Esp32BaseAppConfig::addBool({"board", FaBoardIoConfig::NS, FaBoardIoConfig::KEY_BUTTON_ACTIVE_LOW, "按钮低电平按下", true,
                                 "上拉按钮按下接地时启用。", false, nullptr});
    Esp32BaseAppConfig::addInt({"board", FaBoardIoConfig::NS, FaBoardIoConfig::KEY_DEBOUNCE_MS, "消抖时间", 50, 10, 1000, 10, "ms",
                                "按钮消抖间隔。", false, nullptr});
    Esp32BaseAppConfig::addInt({"board", FaBoardIoConfig::NS, FaBoardIoConfig::KEY_LONG_PRESS_MS, "长按时间", 1000, 200, 10000, 100, "ms",
                                "长按判定时间。", false, nullptr});
    Esp32BaseAppConfig::addBool({"notify", FaNotificationConfig::NS, FaNotificationConfig::KEY_ENABLED, "启用通知规则", true,
                                 "当前只保存规则，尚未接入外部发送通道。", false, nullptr});
    Esp32BaseAppConfig::addBool({"notify", FaNotificationConfig::NS, FaNotificationConfig::KEY_ACTION_DONE, "动作完成", false,
                                 "下料或门控动作完成时通知。", false, nullptr});
    Esp32BaseAppConfig::addBool({"notify", FaNotificationConfig::NS, FaNotificationConfig::KEY_ACTION_FAILED, "动作失败", true,
                                 "动作失败或被本地保护停止时通知。", false, nullptr});
    Esp32BaseAppConfig::addBool({"notify", FaNotificationConfig::NS, FaNotificationConfig::KEY_STATION_FAULT, "分站故障", true,
                                 "分站回报故障时通知。", false, nullptr});
    Esp32BaseAppConfig::addBool({"notify", FaNotificationConfig::NS, FaNotificationConfig::KEY_STATION_OFFLINE, "分站离线", true,
                                 "已配置分站离线时通知。", false, nullptr});
    Esp32BaseAppConfig::addBool({"notify", FaNotificationConfig::NS, FaNotificationConfig::KEY_SCHEDULE_SKIPPED, "计划跳过", true,
                                 "自动计划被跳过时通知。", false, nullptr});
    Esp32BaseAppConfig::addBool({"notify", FaNotificationConfig::NS, FaNotificationConfig::KEY_POWER_RESTORED, "上电恢复", true,
                                 "重启或断电恢复后通知。", false, nullptr});
    Esp32BaseAppConfig::addInt({"rs485", FaRs485Config::NS, FaRs485Config::KEY_MODE, "模式", FA_RS485_MODE_SIMULATED, 0, 2, 1, nullptr,
                                "0 停用，1 真实串口，2 模拟地址 1 和 2 的分站。", true, nullptr});
    Esp32BaseAppConfig::addInt({"rs485", FaRs485Config::NS, FaRs485Config::KEY_UART, "UART", 2, 1, 2, 1, nullptr,
                                "真实串口模式使用的 ESP32 硬件串口。", true, nullptr});
    Esp32BaseAppConfig::addInt({"rs485", FaRs485Config::NS, FaRs485Config::KEY_RX_PIN, "RX 引脚", -1, -1, 39, 1, nullptr,
                                "-1 表示停用 RS485 通讯。", true, nullptr});
    Esp32BaseAppConfig::addInt({"rs485", FaRs485Config::NS, FaRs485Config::KEY_TX_PIN, "TX 引脚", -1, -1, 39, 1, nullptr,
                                "-1 表示停用 RS485 通讯；GPIO34-39 只能输入，不能作为 TX。", true, nullptr});
    Esp32BaseAppConfig::addInt({"rs485", FaRs485Config::NS, FaRs485Config::KEY_DE_PIN, "DE 引脚", -1, -1, 39, 1, nullptr,
                                "485 方向控制引脚；GPIO34-39 只能输入，不能作为 DE。", true, nullptr});
    Esp32BaseAppConfig::addInt({"rs485", FaRs485Config::NS, FaRs485Config::KEY_BAUD, "波特率", 115200, 9600, 1000000, 1, "bps",
                                "默认总线速率为 115200。", true, nullptr});
    Esp32BaseAppConfig::addInt({"rs485", FaRs485Config::NS, FaRs485Config::KEY_TIMEOUT_MS, "超时", 80, 20, 2000, 1, "ms",
                                "单次请求超时时间。", true, nullptr});
}

void fa_master_web_register_routes(FaFeedService *feed_service,
                                   FaDoorService *door_service,
                                   FaDeviceRegistry *device_registry,
                                   FaRs485Master *rs485_master,
                                   FaRs485Transport *transport,
                                   FaMasterActionRuntime *action_runtime,
                                   FaAutoScheduler *auto_scheduler,
                                   FaEnvSensorService *env_sensor,
                                   FaBoardIoService *board_io) {
    g_feed_service = feed_service;
    g_door_service = door_service;
    g_device_registry = device_registry;
    g_rs485_master = rs485_master;
    g_transport = transport;
    g_action_runtime = action_runtime;
    g_auto_scheduler = auto_scheduler;
    g_env_sensor = env_sensor;
    g_board_io = board_io;
    registerRouteChecked("/", Esp32BaseWeb::METHOD_GET, redirectV3Home);
    registerPageChecked("/home", "首页", sendV3HomePage);
    registerPageChecked("/auto", "自动", sendV3AutoPage);
    registerPageChecked("/manual", "手动", sendV3ManualPage);
    registerPageChecked("/records", "记录", sendV3RecordsPage);
    registerPageChecked("/settings", "设置", sendV3SettingsPage);
    registerRouteChecked("/feed", Esp32BaseWeb::METHOD_GET, redirectV3Manual);
    registerRouteChecked("/door", Esp32BaseWeb::METHOD_GET, redirectV3Manual);
    registerRouteChecked("/env", Esp32BaseWeb::METHOD_GET, redirectV3Records);
    registerRouteChecked("/board", Esp32BaseWeb::METHOD_GET, redirectV3Settings);
    registerRouteChecked("/devices", Esp32BaseWeb::METHOD_GET, redirectV3Settings);
    registerRouteChecked("/notify", Esp32BaseWeb::METHOD_GET, redirectV3Settings);
    registerRouteChecked("/bus", Esp32BaseWeb::METHOD_GET, redirectV3Settings);
    registerApiChecked("/api/feed/manual", sendManualFeedApi);
    registerApiChecked("/api/door/open", sendDoorOpenApi);
    registerApiChecked("/api/door/close", sendDoorCloseApi);
    registerApiChecked("/api/door/stop", sendDoorStopApi);
    registerApiChecked("/api/auto/feed-pause", sendAutoFeedPauseApi);
    registerApiChecked("/api/auto/feed-resume", sendAutoFeedResumeApi);
    registerApiChecked("/api/auto/door-pause", sendAutoDoorPauseApi);
    registerApiChecked("/api/auto/door-resume", sendAutoDoorResumeApi);
    registerApiChecked("/api/auto/schedule", sendAutoScheduleSaveApi);
    registerApiChecked("/api/config/feed", sendFeedConfigSaveApi);
    registerApiChecked("/api/config/door", sendDoorConfigSaveApi);
    registerApiChecked("/api/config/env", sendEnvConfigSaveApi);
    registerApiChecked("/api/config/notify", sendNotifyConfigSaveApi);
    registerApiChecked("/api/env/read-now", sendEnvReadNowApi);
    registerApiChecked("/api/bus/scan", sendBusScanApi);
    registerApiChecked("/api/action/stop-active", sendStopActiveActionApi);
    registerApiChecked("/api/status/summary", sendStatusSummaryApi);
    registerApiChecked("/api/records/recent", sendRecentRecordsApi);
    registerApiChecked("/api/devices/enabled", sendDeviceSetEnabledApi);
    registerApiChecked("/api/devices/name", sendDeviceNameApi);
    registerApiChecked("/api/devices/display-order", sendDeviceDisplayOrderApi);
    registerApiChecked("/api/devices/bind-station", sendDeviceBindStationApi);
    registerApiChecked("/api/stations/enabled", sendStationSetEnabledApi);
    registerApiChecked("/api/stations/clear-fault", sendStationClearFaultApi);
}
