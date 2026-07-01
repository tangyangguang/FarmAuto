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
        snprintf(out, len, "registry unavailable");
        return;
    }
    if (!status.has_device) {
        snprintf(out, len, "config fallback");
        return;
    }
    if (!status.has_station) {
        snprintf(out, len, "station missing");
        return;
    }
    if (status.last_error != 0u) {
        snprintf(out, len, "%s, err %u", stationOnlineStateName(status.station_online_state), status.last_error);
        return;
    }
    snprintf(out, len, "%s", stationOnlineStateName(status.station_online_state));
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
        snprintf(out, len, "%lu/1000 turn", static_cast<unsigned long>(record.amount_value));
    } else if (record.amount_mode == FA_ACTION_RECORD_AMOUNT_PULSES) {
        snprintf(out, len, "%lu pulses", static_cast<unsigned long>(record.amount_value));
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
    Esp32BaseWeb::writeHtmlEscaped(recordStateName(record.state));
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
    Esp32BaseWeb::writeHtmlEscaped(stopReasonName(record.stop_reason));
    Esp32BaseWeb::sendChunk("</td><td>");
    Esp32BaseWeb::writeHtmlEscaped(faultName(record.fault_code));
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
    Esp32BaseWeb::beginPanel("Active action");
    Esp32BaseWeb::sendInfoRowCompact("Action", "Currently tracked by master polling.", recordStateName(active->state));
    Esp32BaseWeb::sendInfoRowCompact("Device", "Business device that started this action.", device);
    Esp32BaseWeb::sendInfoRowCompact("Amount", "Original manual request.", amount);
    char progress[36];
    snprintf(progress, sizeof(progress), "%lu / %lu pulses",
             static_cast<unsigned long>(active->completed_pulses),
             static_cast<unsigned long>(active->target_pulses));
    Esp32BaseWeb::sendInfoRowCompact("Progress", "Last status returned by station.", progress);
    char run[20];
    char current[28];
    formatDurationMs(active->run_ms, run, sizeof(run));
    snprintf(current, sizeof(current), "%u / %u mA", active->current_ma, active->peak_current_ma);
    Esp32BaseWeb::sendInfoRowCompact("Run", "Runtime reported by station.", run);
    Esp32BaseWeb::sendInfoRowCompact("Current", "Current / peak current.", current);
    Esp32BaseWeb::sendInfoRowCompact("Stop", "Last station stop reason.", stopReasonName(active->stop_reason));
    Esp32BaseWeb::sendInfoRowCompact("Fault", "Last station fault code.", faultName(active->fault_code));
    Esp32BaseWeb::sendInfoRowCompact("Last error", "Last master-side polling error.", g_action_runtime->lastError());
    Esp32BaseWeb::sendChunk("<div class='actions'><form method='post' action='/api/action/stop-active' onsubmit='return once(this)'><input class='danger' type='submit' value='Stop active'></form></div>");
    Esp32BaseWeb::endPanel();
}

void sendRecentRecordsPanel(void) {
    Esp32BaseWeb::beginPanel("Recent records");
    if (!FaActionRecordStore::isReady()) {
        Esp32BaseWeb::sendNotice(Esp32BaseWeb::UI_WARN, "Records unavailable", "LittleFS action record store is not ready.");
        Esp32BaseWeb::endPanel();
        return;
    }
    const uint16_t count = FaActionRecordStore::count();
    if (count == 0u) {
        Esp32BaseWeb::sendInfoRowCompact("No records", "Completed or failed actions will appear here.");
        Esp32BaseWeb::endPanel();
        return;
    }

    const uint16_t limit = count < kRecentRecordLimit ? count : kRecentRecordLimit;
    Esp32BaseWeb::sendChunk("<div class='tablewrap'><table class='evtable'><thead><tr><th>ID</th><th>Device</th><th>State</th><th>Started</th><th>Amount</th><th>Pulses</th><th>Run</th><th>Addr</th><th>Stop</th><th>Fault</th></tr></thead><tbody>");
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
    Esp32BaseWeb::sendHeader("Records");
    Esp32BaseWeb::sendPageTitle("Records", "Recent FarmAuto action records and the currently tracked action.");

    Esp32BaseWeb::beginMetricGrid();
    snprintf(value, sizeof(value), "%u", FaActionRecordStore::count());
    Esp32BaseWeb::sendMetric("Records", value, FaActionRecordStore::isReady() ? "ready" : "unavailable");
    snprintf(value, sizeof(value), "%u", FaActionRecordStore::capacity());
    Esp32BaseWeb::sendMetric("Capacity", value);
    snprintf(value, sizeof(value), "%lu", static_cast<unsigned long>(FaActionRecordStore::sequence()));
    Esp32BaseWeb::sendMetric("Sequence", value);
    Esp32BaseWeb::sendMetric("Action", g_action_runtime != nullptr && g_action_runtime->isBusy() ? "running" : "idle");
    Esp32BaseWeb::endMetricGrid();

    sendActiveActionPanel();
    sendRecentRecordsPanel();
    Esp32BaseWeb::sendFooter();
}

void fa_master_web_register_config(void) {
    Esp32BaseWeb::setDeviceName("FarmAuto");
    Esp32BaseWeb::setHomePath("/feed");
    Esp32BaseWeb::setHomeMode(Esp32BaseWeb::HOME_COMBINED);
    Esp32BaseWeb::setSystemNavMode(Esp32BaseWeb::SYSTEM_NAV_SECTION);
    Esp32BaseAppConfig::setTitle("FarmAuto Config");
    Esp32BaseAppConfig::addGroup({"feeder", "Feeder"});
    Esp32BaseAppConfig::addGroup({"door", "Door"});
    Esp32BaseAppConfig::addGroup({"auto", "Auto"});
    Esp32BaseAppConfig::addGroup({"env", "Environment"});
    Esp32BaseAppConfig::addGroup({"notify", "Notify"});
    Esp32BaseAppConfig::addGroup({"rs485", "RS485"});
    Esp32BaseAppConfig::addInt({"feeder", kNs, kStationAddress, "Station address", 1, 1, 127, 1, nullptr,
                                "RS485 address 1..127.", false, nullptr});
    Esp32BaseAppConfig::addInt({"feeder", kNs, kPulsesPerTurn, "Pulses per turn", 4320, 1, 200000, 1, "pulses",
                                "Output shaft encoder pulses per turn.", false, nullptr});
    Esp32BaseAppConfig::addInt({"feeder", kNs, kGramsPerTurnMg, "mg per turn", 8000, 1, 1000000, 1, "mg",
                                "Calibration for mg based feeding.", false, nullptr});
    Esp32BaseAppConfig::addInt({"feeder", kNs, kDirection, "Direction", 1, -1, 1, 1, nullptr,
                                "1 forward, -1 reverse.", false, nullptr});
    Esp32BaseAppConfig::addInt({"feeder", kNs, kSpeedPermille, "Speed", 800, 1, 1000, 1, "permille",
                                "Motor speed request.", false, nullptr});
    Esp32BaseAppConfig::addInt({"feeder", kNs, kOverCurrentMa, "Over-current", 2000, 1, 10000, 1, "mA",
                                "Station protection threshold.", false, nullptr});
    Esp32BaseAppConfig::addInt({"feeder", kNs, kMaxRunMs, "Max run", 60000, 100, 600000, 100, "ms",
                                "Single action timeout.", false, nullptr});
    Esp32BaseAppConfig::addInt({"feeder", kNs, kMaxActionPulses, "Max pulses", 432000, 1, 2000000, 1, "pulses",
                                "Single action pulse limit.", false, nullptr});
    Esp32BaseAppConfig::addInt({"door", kDoorNs, kDoorStationAddress, "Station address", 2, 1, 127, 1, nullptr,
                                "RS485 address 1..127.", false, nullptr});
    Esp32BaseAppConfig::addInt({"door", kDoorNs, kDoorPulsesPerTurn, "Pulses per turn", 4320, 1, 200000, 1, "pulses",
                                "Output shaft encoder pulses per turn.", false, nullptr});
    Esp32BaseAppConfig::addInt({"door", kDoorNs, kDoorTravelPulses, "Travel pulses", 20000, 1, 2000000, 1, "pulses",
                                "Bounded travel for one open or close action.", false, nullptr});
    Esp32BaseAppConfig::addInt({"door", kDoorNs, kDoorOpenDirection, "Open direction", 1, -1, 1, 1, nullptr,
                                "1 forward, -1 reverse.", false, nullptr});
    Esp32BaseAppConfig::addInt({"door", kDoorNs, kDoorCloseDirection, "Close direction", -1, -1, 1, 1, nullptr,
                                "1 forward, -1 reverse.", false, nullptr});
    Esp32BaseAppConfig::addInt({"door", kDoorNs, kDoorSpeedPermille, "Speed", 700, 1, 1000, 1, "permille",
                                "Motor speed request.", false, nullptr});
    Esp32BaseAppConfig::addInt({"door", kDoorNs, kDoorOverCurrentMa, "Over-current", 2500, 1, 10000, 1, "mA",
                                "Station protection threshold.", false, nullptr});
    Esp32BaseAppConfig::addInt({"door", kDoorNs, kDoorMaxRunMs, "Max run", 30000, 100, 600000, 100, "ms",
                                "Single action timeout.", false, nullptr});
    Esp32BaseAppConfig::addInt({"door", kDoorNs, kDoorMaxActionPulses, "Max pulses", 100000, 1, 2000000, 1, "pulses",
                                "Single action pulse limit.", false, nullptr});
    Esp32BaseAppConfig::addInt({"auto", FaAutoScheduleConfig::NS, FaAutoScheduleConfig::KEY_ENABLED, "Auto enabled", 1, 0, 1, 1, nullptr,
                                "1 enables daily automatic schedules.", false, nullptr});
    Esp32BaseAppConfig::addInt({"auto", FaAutoScheduleConfig::NS, FaAutoScheduleConfig::KEY_TZ_OFFSET_MIN, "Timezone offset", 480, -720, 840, 1, "min",
                                "Local time offset from UTC; China is 480.", false, nullptr});
    Esp32BaseAppConfig::addInt({"auto", FaAutoScheduleConfig::NS, FaAutoScheduleConfig::KEY_FEED_ENABLED, "Feed schedule", 1, 0, 1, 1, nullptr,
                                "1 enables daily feed points.", false, nullptr});
    Esp32BaseAppConfig::addInt({"auto", FaAutoScheduleConfig::NS, FaAutoScheduleConfig::KEY_FEED_1_MIN, "Feed 1 minute", 430, 0, 1439, 1, "min",
                                "Minute of local day, e.g. 430 is 07:10.", false, nullptr});
    Esp32BaseAppConfig::addInt({"auto", FaAutoScheduleConfig::NS, FaAutoScheduleConfig::KEY_FEED_1_AMOUNT_MG, "Feed 1 amount", 100000, 1, 5000000, 1, "mg",
                                "Scheduled feed amount.", false, nullptr});
    Esp32BaseAppConfig::addInt({"auto", FaAutoScheduleConfig::NS, FaAutoScheduleConfig::KEY_FEED_2_MIN, "Feed 2 minute", 1090, 0, 1439, 1, "min",
                                "Minute of local day, e.g. 1090 is 18:10.", false, nullptr});
    Esp32BaseAppConfig::addInt({"auto", FaAutoScheduleConfig::NS, FaAutoScheduleConfig::KEY_FEED_2_AMOUNT_MG, "Feed 2 amount", 100000, 1, 5000000, 1, "mg",
                                "Scheduled feed amount.", false, nullptr});
    Esp32BaseAppConfig::addInt({"auto", FaAutoScheduleConfig::NS, FaAutoScheduleConfig::KEY_DOOR_ENABLED, "Door schedule", 1, 0, 1, 1, nullptr,
                                "1 enables daily door open/close.", false, nullptr});
    Esp32BaseAppConfig::addInt({"auto", FaAutoScheduleConfig::NS, FaAutoScheduleConfig::KEY_DOOR_OPEN_MIN, "Door open minute", 480, 0, 1439, 1, "min",
                                "Minute of local day, e.g. 480 is 08:00.", false, nullptr});
    Esp32BaseAppConfig::addInt({"auto", FaAutoScheduleConfig::NS, FaAutoScheduleConfig::KEY_DOOR_CLOSE_MIN, "Door close minute", 1050, 0, 1439, 1, "min",
                                "Minute of local day, e.g. 1050 is 17:30.", false, nullptr});
    Esp32BaseAppConfig::addInt({"auto", FaAutoScheduleConfig::NS, FaAutoScheduleConfig::KEY_FEED_PAUSE_UNTIL, "Feed pause until", 0, 0, 2147483647, 1, "epoch",
                                "0 means not paused; epoch seconds pauses automatic feed.", false, nullptr});
    Esp32BaseAppConfig::addInt({"auto", FaAutoScheduleConfig::NS, FaAutoScheduleConfig::KEY_DOOR_PAUSE_UNTIL, "Door pause until", 0, 0, 2147483647, 1, "epoch",
                                "0 means not paused; epoch seconds pauses automatic door.", false, nullptr});
    Esp32BaseAppConfig::addBool({"env", FaEnvSensorConfig::NS, FaEnvSensorConfig::KEY_ENABLED, "SHT30 enabled", true,
                                 "Read outdoor temperature and humidity from SHT30.", false, nullptr});
    Esp32BaseAppConfig::addInt({"env", FaEnvSensorConfig::NS, FaEnvSensorConfig::KEY_SDA_PIN, "SDA pin", 21, -1, 39, 1, nullptr,
                                "-1 disables SHT30 I2C.", false, nullptr});
    Esp32BaseAppConfig::addInt({"env", FaEnvSensorConfig::NS, FaEnvSensorConfig::KEY_SCL_PIN, "SCL pin", 22, -1, 39, 1, nullptr,
                                "-1 disables SHT30 I2C.", false, nullptr});
    Esp32BaseAppConfig::addInt({"env", FaEnvSensorConfig::NS, FaEnvSensorConfig::KEY_ADDRESS, "I2C address", 68, 8, 119, 1, nullptr,
                                "SHT30 7-bit address, usually 68 decimal for 0x44.", false, nullptr});
    Esp32BaseAppConfig::addInt({"env", FaEnvSensorConfig::NS, FaEnvSensorConfig::KEY_INTERVAL_MS, "Sample interval", 5000, 1000, 600000, 1000, "ms",
                                "Background sampling interval.", false, nullptr});
    Esp32BaseAppConfig::addInt({"env", FaEnvSensorConfig::NS, FaEnvSensorConfig::KEY_RECORD_INTERVAL_S, "Record interval", 300, 10, 86400, 10, "s",
                                "Minimum interval for app event records.", false, nullptr});
    Esp32BaseAppConfig::addBool({"notify", FaNotificationConfig::NS, FaNotificationConfig::KEY_ENABLED, "Notifications", true,
                                 "Only stores rules; no external sender is connected yet.", false, nullptr});
    Esp32BaseAppConfig::addBool({"notify", FaNotificationConfig::NS, FaNotificationConfig::KEY_ACTION_DONE, "Action completed", false,
                                 "Notify when a feed or door action completes.", false, nullptr});
    Esp32BaseAppConfig::addBool({"notify", FaNotificationConfig::NS, FaNotificationConfig::KEY_ACTION_FAILED, "Action failed", true,
                                 "Notify when an action fails or is locally protected.", false, nullptr});
    Esp32BaseAppConfig::addBool({"notify", FaNotificationConfig::NS, FaNotificationConfig::KEY_STATION_FAULT, "Station fault", true,
                                 "Notify when a station reports a fault.", false, nullptr});
    Esp32BaseAppConfig::addBool({"notify", FaNotificationConfig::NS, FaNotificationConfig::KEY_STATION_OFFLINE, "Station offline", true,
                                 "Notify when a configured station is offline.", false, nullptr});
    Esp32BaseAppConfig::addBool({"notify", FaNotificationConfig::NS, FaNotificationConfig::KEY_SCHEDULE_SKIPPED, "Schedule skipped", true,
                                 "Notify when an automatic plan is skipped.", false, nullptr});
    Esp32BaseAppConfig::addBool({"notify", FaNotificationConfig::NS, FaNotificationConfig::KEY_POWER_RESTORED, "Power restored", true,
                                 "Notify after restart or power recovery.", false, nullptr});
    Esp32BaseAppConfig::addInt({"rs485", FaRs485Config::NS, FaRs485Config::KEY_MODE, "Mode", FA_RS485_MODE_SIMULATED, 0, 2, 1, nullptr,
                                "0 disabled, 1 real UART, 2 simulated stations 1 and 2.", true, nullptr});
    Esp32BaseAppConfig::addInt({"rs485", FaRs485Config::NS, FaRs485Config::KEY_UART, "UART", 2, 1, 2, 1, nullptr,
                                "ESP32 hardware serial port for real UART mode.", true, nullptr});
    Esp32BaseAppConfig::addInt({"rs485", FaRs485Config::NS, FaRs485Config::KEY_RX_PIN, "RX pin", -1, -1, 39, 1, nullptr,
                                "-1 disables RS485 transport.", true, nullptr});
    Esp32BaseAppConfig::addInt({"rs485", FaRs485Config::NS, FaRs485Config::KEY_TX_PIN, "TX pin", -1, -1, 39, 1, nullptr,
                                "-1 disables RS485 transport; GPIO34-39 are input-only and invalid here.", true, nullptr});
    Esp32BaseAppConfig::addInt({"rs485", FaRs485Config::NS, FaRs485Config::KEY_DE_PIN, "DE pin", -1, -1, 39, 1, nullptr,
                                "Driver enable pin; GPIO34-39 are input-only and invalid here.", true, nullptr});
    Esp32BaseAppConfig::addInt({"rs485", FaRs485Config::NS, FaRs485Config::KEY_BAUD, "Baud", 115200, 9600, 1000000, 1, "bps",
                                "Default bus rate is 115200.", true, nullptr});
    Esp32BaseAppConfig::addInt({"rs485", FaRs485Config::NS, FaRs485Config::KEY_TIMEOUT_MS, "Timeout", 80, 20, 2000, 1, "ms",
                                "Single request timeout.", true, nullptr});
}

void fa_master_web_register_routes(FaFeedService *feed_service,
                                   FaDoorService *door_service,
                                   FaDeviceRegistry *device_registry,
                                   FaRs485Master *rs485_master,
                                   FaRs485Transport *transport,
                                   FaMasterActionRuntime *action_runtime,
                                   FaAutoScheduler *auto_scheduler,
                                   FaEnvSensorService *env_sensor) {
    g_feed_service = feed_service;
    g_door_service = door_service;
    g_device_registry = device_registry;
    g_rs485_master = rs485_master;
    g_transport = transport;
    g_action_runtime = action_runtime;
    g_auto_scheduler = auto_scheduler;
    g_env_sensor = env_sensor;
    Esp32BaseWeb::addPage("/feed", "Feed", sendFeedPage);
    Esp32BaseWeb::addPage("/door", "Door", sendDoorPage);
    Esp32BaseWeb::addPage("/auto", "Auto", sendAutoPage);
    Esp32BaseWeb::addPage("/env", "Env", sendEnvPage);
    Esp32BaseWeb::addPage("/records", "Records", sendRecordsPage);
    Esp32BaseWeb::addPage("/devices", "Devices", sendDevicesPage);
    Esp32BaseWeb::addPage("/notify", "Notify", sendNotifyPage);
    Esp32BaseWeb::addPage("/bus", "RS485", sendBusPage);
    Esp32BaseWeb::addApi("/api/feed/manual", sendManualFeedApi);
    Esp32BaseWeb::addApi("/api/door/open", sendDoorOpenApi);
    Esp32BaseWeb::addApi("/api/door/close", sendDoorCloseApi);
    Esp32BaseWeb::addApi("/api/door/stop", sendDoorStopApi);
    Esp32BaseWeb::addApi("/api/auto/feed-pause", sendAutoFeedPauseApi);
    Esp32BaseWeb::addApi("/api/auto/feed-resume", sendAutoFeedResumeApi);
    Esp32BaseWeb::addApi("/api/auto/door-pause", sendAutoDoorPauseApi);
    Esp32BaseWeb::addApi("/api/auto/door-resume", sendAutoDoorResumeApi);
    Esp32BaseWeb::addApi("/api/env/read-now", sendEnvReadNowApi);
    Esp32BaseWeb::addApi("/api/bus/scan", sendBusScanApi);
    Esp32BaseWeb::addApi("/api/action/stop-active", sendStopActiveActionApi);
    Esp32BaseWeb::addApi("/api/devices/enabled", sendDeviceSetEnabledApi);
    Esp32BaseWeb::addApi("/api/devices/name", sendDeviceNameApi);
    Esp32BaseWeb::addApi("/api/devices/display-order", sendDeviceDisplayOrderApi);
    Esp32BaseWeb::addApi("/api/devices/bind-station", sendDeviceBindStationApi);
    Esp32BaseWeb::addApi("/api/stations/enabled", sendStationSetEnabledApi);
    Esp32BaseWeb::addApi("/api/stations/clear-fault", sendStationClearFaultApi);
}
