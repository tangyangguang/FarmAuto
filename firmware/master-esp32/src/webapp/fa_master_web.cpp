#include "fa_master_web_internal.h"

#include <stdlib.h>
#include <string.h>

FaFeedService* g_feed_service = nullptr;
FaDoorService* g_door_service = nullptr;
FaDeviceRegistry* g_device_registry = nullptr;
FaRs485Master* g_rs485_master = nullptr;
FaRs485Transport* g_transport = nullptr;
FaMasterActionRuntime* g_action_runtime = nullptr;

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
    formatDeviceLabel(record.device_id, device, sizeof(device));

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
    formatDeviceLabel(active->device_id, device, sizeof(device));
    Esp32BaseWeb::beginPanel("Active action");
    Esp32BaseWeb::sendInfoRowCompact("Action", "Currently tracked by master polling.", recordStateName(active->state));
    Esp32BaseWeb::sendInfoRowCompact("Device", "Business device that started this action.", device);
    Esp32BaseWeb::sendInfoRowCompact("Amount", "Original manual request.", amount);
    char progress[36];
    snprintf(progress, sizeof(progress), "%lu / %lu pulses",
             static_cast<unsigned long>(active->completed_pulses),
             static_cast<unsigned long>(active->target_pulses));
    Esp32BaseWeb::sendInfoRowCompact("Progress", "Last status returned by station.", progress);
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

    Esp32BaseWeb::beginJson(200);
    Esp32BaseWeb::sendChunk("\"ok\":true,\"command\":\"stop_active\",\"actionId\":");
    sendNumber(active->action_id);
    Esp32BaseWeb::sendChunk(",\"stationAddress\":");
    sendNumber(station_address);
    Esp32BaseWeb::sendChunk(",\"message\":\"stop accepted by active station\"");
    Esp32BaseWeb::endJson();
}

void fa_master_web_register_config(void) {
    Esp32BaseWeb::setDeviceName("FarmAuto");
    Esp32BaseWeb::setHomePath("/feed");
    Esp32BaseWeb::setHomeMode(Esp32BaseWeb::HOME_COMBINED);
    Esp32BaseWeb::setSystemNavMode(Esp32BaseWeb::SYSTEM_NAV_SECTION);
    Esp32BaseAppConfig::setTitle("FarmAuto Config");
    Esp32BaseAppConfig::addGroup({"feeder", "Feeder"});
    Esp32BaseAppConfig::addGroup({"door", "Door"});
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
    Esp32BaseAppConfig::addInt({"rs485", FaRs485Config::NS, FaRs485Config::KEY_UART, "UART", 2, 1, 2, 1, nullptr,
                                "ESP32 hardware serial port.", true, nullptr});
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
                                   FaMasterActionRuntime *action_runtime) {
    g_feed_service = feed_service;
    g_door_service = door_service;
    g_device_registry = device_registry;
    g_rs485_master = rs485_master;
    g_transport = transport;
    g_action_runtime = action_runtime;
    Esp32BaseWeb::addPage("/feed", "Feed", sendFeedPage);
    Esp32BaseWeb::addPage("/door", "Door", sendDoorPage);
    Esp32BaseWeb::addPage("/devices", "Devices", sendDevicesPage);
    Esp32BaseWeb::addPage("/bus", "RS485", sendBusPage);
    Esp32BaseWeb::addApi("/api/feed/manual", sendManualFeedApi);
    Esp32BaseWeb::addApi("/api/door/open", sendDoorOpenApi);
    Esp32BaseWeb::addApi("/api/door/close", sendDoorCloseApi);
    Esp32BaseWeb::addApi("/api/door/stop", sendDoorStopApi);
    Esp32BaseWeb::addApi("/api/bus/scan", sendBusScanApi);
    Esp32BaseWeb::addApi("/api/action/stop-active", sendStopActiveActionApi);
    Esp32BaseWeb::addApi("/api/devices/enabled", sendDeviceSetEnabledApi);
}
