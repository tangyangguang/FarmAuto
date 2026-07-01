#include "fa_master_web_internal.h"

namespace {

const char* deviceTypeName(uint8_t type) {
    switch (type) {
    case FA_DEVICE_TYPE_DOOR:
        return "门控";
    case FA_DEVICE_TYPE_FEEDER:
        return "下料";
    default:
        return "未知";
    }
}

bool deviceSortsBefore(const FaDeviceRecord& left, const FaDeviceRecord& right) {
    if (left.sort_order != right.sort_order) {
        return left.sort_order < right.sort_order;
    }
    return left.device_id < right.device_id;
}

void sendDeviceRow(const FaDeviceRecord& device) {
    FaStationRecord station;
    const bool hasStation = g_device_registry != nullptr && g_device_registry->stationById(device.station_id, station);

    Esp32BaseWeb::sendChunk("<tr><td>");
    sendNumber(device.device_id);
    Esp32BaseWeb::sendChunk("</td><td>");
    Esp32BaseWeb::writeHtmlEscaped(device.name);
    Esp32BaseWeb::sendChunk("</td><td>");
    Esp32BaseWeb::writeHtmlEscaped(deviceTypeName(device.type));
    Esp32BaseWeb::sendChunk("</td><td>");
    sendNumber(device.display_no);
    Esp32BaseWeb::sendChunk("</td><td>");
    sendNumber(device.sort_order);
    Esp32BaseWeb::sendChunk("</td><td>");
    sendNumber(device.station_id);
    Esp32BaseWeb::sendChunk(" / ");
    if (hasStation) {
        sendNumber(station.bus_address);
    } else {
        Esp32BaseWeb::sendChunk("-");
    }
    Esp32BaseWeb::sendChunk("</td><td>");
    Esp32BaseWeb::writeHtmlEscaped(hasStation ? uiStationOnlineState(station.online_state) : "未绑定分站");
    Esp32BaseWeb::sendChunk("</td><td>");
    Esp32BaseWeb::writeHtmlEscaped(uiEnabled(device.enabled != 0u));
    Esp32BaseWeb::sendChunk("</td><td><form method='post' action='/api/devices/name' onsubmit='return once(this)'>");
    Esp32BaseWeb::sendChunk("<input type='hidden' name='deviceId' value='");
    sendNumber(device.device_id);
    Esp32BaseWeb::sendChunk("'><input name='name' maxlength='23' value='");
    Esp32BaseWeb::writeHtmlEscaped(device.name);
    Esp32BaseWeb::sendChunk("'><input type='submit' value='改名'></form>");
    Esp32BaseWeb::sendChunk("<form method='post' action='/api/devices/display-order' onsubmit='return once(this)'>");
    Esp32BaseWeb::sendChunk("<input type='hidden' name='deviceId' value='");
    sendNumber(device.device_id);
    Esp32BaseWeb::sendChunk("'><input type='number' name='displayNo' min='1' max='9999' value='");
    sendNumber(device.display_no);
    Esp32BaseWeb::sendChunk("'><input type='number' name='sortOrder' min='0' max='65535' value='");
    sendNumber(device.sort_order);
    Esp32BaseWeb::sendChunk("'><input type='submit' value='排序'></form>");
    Esp32BaseWeb::sendChunk("<form method='post' action='/api/devices/enabled' onsubmit='return once(this)'>");
    Esp32BaseWeb::sendChunk("<input type='hidden' name='deviceId' value='");
    sendNumber(device.device_id);
    Esp32BaseWeb::sendChunk("'><input type='hidden' name='enabled' value='");
    Esp32BaseWeb::sendChunk(device.enabled != 0u ? "0" : "1");
    Esp32BaseWeb::sendChunk("'><input type='submit' value='");
    Esp32BaseWeb::sendChunk(device.enabled != 0u ? "停用" : "启用");
    Esp32BaseWeb::sendChunk("'></form>");
    Esp32BaseWeb::sendChunk("<form method='post' action='/api/devices/bind-station' onsubmit='return once(this)'>");
    Esp32BaseWeb::sendChunk("<input type='hidden' name='deviceId' value='");
    sendNumber(device.device_id);
    Esp32BaseWeb::sendChunk("'><input type='number' name='address' min='1' max='127' value='");
    if (hasStation) {
        sendNumber(station.bus_address);
    } else {
        Esp32BaseWeb::sendChunk("1");
    }
    Esp32BaseWeb::sendChunk("'><input type='submit' value='绑定'></form></td></tr>");
}

void sendStationRow(const FaStationRecord& station) {
    Esp32BaseWeb::sendChunk("<tr><td>");
    sendNumber(station.station_id);
    Esp32BaseWeb::sendChunk("</td><td>");
    sendNumber(station.bus_address);
    Esp32BaseWeb::sendChunk("</td><td>");
    Esp32BaseWeb::writeHtmlEscaped(uiEnabled(station.enabled != 0u));
    Esp32BaseWeb::sendChunk("</td><td>");
    Esp32BaseWeb::writeHtmlEscaped(uiStationOnlineState(station.online_state));
    Esp32BaseWeb::sendChunk("</td><td>");
    sendNumber(station.protocol_version);
    Esp32BaseWeb::sendChunk("</td><td>");
    sendNumber(station.firmware_version);
    Esp32BaseWeb::sendChunk("</td><td>");
    sendNumber(station.capability_flags);
    Esp32BaseWeb::sendChunk("</td><td>");
    sendNumber(station.last_seen_at);
    Esp32BaseWeb::sendChunk("</td><td>");
    sendNumber(station.last_error);
    Esp32BaseWeb::sendChunk("</td><td><form method='post' action='/api/stations/enabled' onsubmit='return once(this)'>");
    Esp32BaseWeb::sendChunk("<input type='hidden' name='address' value='");
    sendNumber(station.bus_address);
    Esp32BaseWeb::sendChunk("'><input type='hidden' name='enabled' value='");
    Esp32BaseWeb::sendChunk(station.enabled != 0u ? "0" : "1");
    Esp32BaseWeb::sendChunk("'><input type='submit' value='");
    Esp32BaseWeb::sendChunk(station.enabled != 0u ? "停用" : "启用");
    Esp32BaseWeb::sendChunk("'></form><form method='post' action='/api/stations/clear-fault' onsubmit='return once(this)'>");
    Esp32BaseWeb::sendChunk("<input type='hidden' name='address' value='");
    sendNumber(station.bus_address);
    Esp32BaseWeb::sendChunk("'><input type='submit' value='清故障'></form></td></tr>");
}

}  // namespace

void sendDevicesPage(void) {
    if (!Esp32BaseWeb::checkAuth()) {
        return;
    }

    char value[24];
    Esp32BaseWeb::sendHeader("设备");
    Esp32BaseWeb::sendPageTitle("设备管理", "管理业务设备及其绑定的 RS485 分站。");

    Esp32BaseWeb::beginMetricGrid();
    Esp32BaseWeb::sendMetric("设备表", g_device_registry != nullptr && g_device_registry->isReady() ? "就绪" : "不可用");
    snprintf(value, sizeof(value), "%u", g_device_registry != nullptr ? g_device_registry->deviceCount() : 0u);
    Esp32BaseWeb::sendMetric("设备数", value);
    snprintf(value, sizeof(value), "%u", g_device_registry != nullptr ? g_device_registry->stationCount() : 0u);
    Esp32BaseWeb::sendMetric("分站数", value);
    snprintf(value, sizeof(value), "%lu", static_cast<unsigned long>(g_device_registry != nullptr ? g_device_registry->sequence() : 0u));
    Esp32BaseWeb::sendMetric("序号", value);
    Esp32BaseWeb::endMetricGrid();

    if (g_device_registry == nullptr || !g_device_registry->isReady()) {
        Esp32BaseWeb::sendNotice(Esp32BaseWeb::UI_WARN, "设备表不可用", "LittleFS 设备表未就绪。");
        Esp32BaseWeb::sendFooter();
        return;
    }

    Esp32BaseWeb::beginPanel("业务设备");
    Esp32BaseWeb::sendChunk("<div class='tablewrap'><table class='evtable'><thead><tr><th>ID</th><th>名称</th><th>类型</th><th>编号</th><th>排序</th><th>分站 / 地址</th><th>分站状态</th><th>设备状态</th><th>操作</th></tr></thead><tbody>");
    bool sent[FaDeviceRegistry::kMaxDevices] = {};
    const uint8_t deviceCount = g_device_registry->deviceCount();
    for (uint8_t emitted = 0u; emitted < deviceCount; ++emitted) {
        uint8_t bestIndex = 0u;
        bool hasBest = false;
        FaDeviceRecord best;
        for (uint8_t i = 0u; i < deviceCount; ++i) {
            FaDeviceRecord candidate;
            if (!sent[i] && g_device_registry->deviceAt(i, candidate) &&
                (!hasBest || deviceSortsBefore(candidate, best))) {
                best = candidate;
                bestIndex = i;
                hasBest = true;
            }
        }
        if (!hasBest) {
            break;
        }
        sent[bestIndex] = true;
        sendDeviceRow(best);
    }
    Esp32BaseWeb::sendChunk("</tbody></table></div>");
    Esp32BaseWeb::endPanel();

    Esp32BaseWeb::beginPanel("分站");
    Esp32BaseWeb::sendChunk("<div class='tablewrap'><table class='evtable'><thead><tr><th>ID</th><th>地址</th><th>启用</th><th>在线</th><th>协议</th><th>固件</th><th>能力</th><th>最近在线</th><th>错误</th><th>操作</th></tr></thead><tbody>");
    for (uint8_t i = 0u; i < g_device_registry->stationCount(); ++i) {
        FaStationRecord station;
        if (g_device_registry->stationAt(i, station)) {
            sendStationRow(station);
        }
    }
    Esp32BaseWeb::sendChunk("</tbody></table></div>");
    Esp32BaseWeb::endPanel();

    Esp32BaseWeb::sendInfoRowCompactLink("总线发现", "运行 RS485 扫描，刷新分站固件、协议和能力信息。", "RS485", "/bus", "打开", Esp32BaseWeb::UI_INFO);
    Esp32BaseWeb::sendFooter();
}

void sendDeviceSetEnabledApi(void) {
    if (!Esp32BaseWeb::checkPostAllowed("device_enabled")) {
        return;
    }
    if (g_device_registry == nullptr || !g_device_registry->isReady()) {
        Esp32BaseWeb::sendJson(503, "{\"ok\":false,\"error\":\"registry_unavailable\"}");
        return;
    }

    const uint16_t deviceId = static_cast<uint16_t>(readUIntParam("deviceId", 0u));
    const bool enabled = readUIntParam("enabled", 0u) != 0u;
    if (deviceId == 0u) {
        Esp32BaseWeb::sendJson(400, "{\"ok\":false,\"error\":\"bad_device_id\"}");
        return;
    }
    if (!g_device_registry->setDeviceEnabled(deviceId, enabled)) {
        Esp32BaseWeb::sendJson(404, "{\"ok\":false,\"error\":\"device_not_found\"}");
        return;
    }

    Esp32BaseWeb::beginJson(200);
    Esp32BaseWeb::sendChunk("\"ok\":true,\"deviceId\":");
    sendNumber(deviceId);
    Esp32BaseWeb::sendChunk(",\"enabled\":");
    Esp32BaseWeb::sendChunk(enabled ? "true" : "false");
    Esp32BaseWeb::sendChunk(",\"message\":\"device state updated\"");
    Esp32BaseWeb::endJson();
}

void sendDeviceNameApi(void) {
    if (!Esp32BaseWeb::checkPostAllowed("device_name")) {
        return;
    }
    if (g_device_registry == nullptr || !g_device_registry->isReady()) {
        Esp32BaseWeb::sendJson(503, "{\"ok\":false,\"error\":\"registry_unavailable\"}");
        return;
    }

    const uint16_t deviceId = static_cast<uint16_t>(readUIntParam("deviceId", 0u));
    char name[sizeof(FaDeviceRecord().name)] = "";
    (void)Esp32BaseWeb::getParam("name", name, sizeof(name));
    if (deviceId == 0u) {
        Esp32BaseWeb::sendJson(400, "{\"ok\":false,\"error\":\"bad_device_id\"}");
        return;
    }
    if (name[0] == '\0') {
        Esp32BaseWeb::sendJson(400, "{\"ok\":false,\"error\":\"bad_name\"}");
        return;
    }
    if (!g_device_registry->setDeviceName(deviceId, name)) {
        Esp32BaseWeb::sendJson(404, "{\"ok\":false,\"error\":\"device_not_found\"}");
        return;
    }

    ESP32BASE_LOG_I("farm", "device_name_updated device_id=%u name=%s", deviceId, name);
    Esp32BaseWeb::beginJson(200);
    Esp32BaseWeb::sendChunk("\"ok\":true,\"deviceId\":");
    sendNumber(deviceId);
    Esp32BaseWeb::sendChunk(",\"message\":\"device name updated\"");
    Esp32BaseWeb::endJson();
}

void sendDeviceDisplayOrderApi(void) {
    if (!Esp32BaseWeb::checkPostAllowed("device_display_order")) {
        return;
    }
    if (g_device_registry == nullptr || !g_device_registry->isReady()) {
        Esp32BaseWeb::sendJson(503, "{\"ok\":false,\"error\":\"registry_unavailable\"}");
        return;
    }

    const uint16_t deviceId = static_cast<uint16_t>(readUIntParam("deviceId", 0u));
    const uint16_t displayNo = static_cast<uint16_t>(readUIntParam("displayNo", 0u));
    const uint16_t sortOrder = static_cast<uint16_t>(readUIntParam("sortOrder", 0u));
    if (deviceId == 0u) {
        Esp32BaseWeb::sendJson(400, "{\"ok\":false,\"error\":\"bad_device_id\"}");
        return;
    }
    if (displayNo == 0u) {
        Esp32BaseWeb::sendJson(400, "{\"ok\":false,\"error\":\"bad_display_no\"}");
        return;
    }
    if (!g_device_registry->setDeviceDisplayOrder(deviceId, displayNo, sortOrder)) {
        Esp32BaseWeb::sendJson(404, "{\"ok\":false,\"error\":\"device_not_found\"}");
        return;
    }

    ESP32BASE_LOG_I("farm", "device_display_order_updated device_id=%u display_no=%u sort_order=%u",
                    deviceId,
                    displayNo,
                    sortOrder);
    Esp32BaseWeb::beginJson(200);
    Esp32BaseWeb::sendChunk("\"ok\":true,\"deviceId\":");
    sendNumber(deviceId);
    Esp32BaseWeb::sendChunk(",\"displayNo\":");
    sendNumber(displayNo);
    Esp32BaseWeb::sendChunk(",\"sortOrder\":");
    sendNumber(sortOrder);
    Esp32BaseWeb::sendChunk(",\"message\":\"device display order updated\"");
    Esp32BaseWeb::endJson();
}

void sendDeviceBindStationApi(void) {
    if (!Esp32BaseWeb::checkPostAllowed("device_bind_station")) {
        return;
    }
    if (g_device_registry == nullptr || !g_device_registry->isReady()) {
        Esp32BaseWeb::sendJson(503, "{\"ok\":false,\"error\":\"registry_unavailable\"}");
        return;
    }
    if (g_action_runtime != nullptr && g_action_runtime->isBusy()) {
        Esp32BaseWeb::sendJson(409, "{\"ok\":false,\"error\":\"action_busy\"}");
        return;
    }

    const uint16_t deviceId = static_cast<uint16_t>(readUIntParam("deviceId", 0u));
    const uint8_t address = static_cast<uint8_t>(readUIntParam("address", 0u));
    if (deviceId == 0u) {
        Esp32BaseWeb::sendJson(400, "{\"ok\":false,\"error\":\"bad_device_id\"}");
        return;
    }
    if (!fa_address_is_normal(address)) {
        Esp32BaseWeb::sendJson(400, "{\"ok\":false,\"error\":\"bad_address\"}");
        return;
    }

    FaDeviceRecord device;
    if (!g_device_registry->deviceById(deviceId, device)) {
        Esp32BaseWeb::sendJson(404, "{\"ok\":false,\"error\":\"device_not_found\"}");
        return;
    }
    FaStationRecord station;
    if (!g_device_registry->stationByAddress(address, station)) {
        Esp32BaseWeb::sendJson(404, "{\"ok\":false,\"error\":\"station_not_found\"}");
        return;
    }
    if (!g_device_registry->setDeviceStationByAddress(deviceId, address)) {
        Esp32BaseWeb::sendJson(500, "{\"ok\":false,\"error\":\"bind_failed\"}");
        return;
    }

    ESP32BASE_LOG_I("farm", "device_bound device_id=%u station_id=%u addr=%u",
                    deviceId,
                    station.station_id,
                    address);
    Esp32BaseWeb::beginJson(200);
    Esp32BaseWeb::sendChunk("\"ok\":true,\"deviceId\":");
    sendNumber(deviceId);
    Esp32BaseWeb::sendChunk(",\"stationId\":");
    sendNumber(station.station_id);
    Esp32BaseWeb::sendChunk(",\"stationAddress\":");
    sendNumber(address);
    Esp32BaseWeb::sendChunk(",\"message\":\"device station binding updated\"");
    Esp32BaseWeb::endJson();
}

void sendStationSetEnabledApi(void) {
    if (!Esp32BaseWeb::checkPostAllowed("station_enabled")) {
        return;
    }
    if (g_device_registry == nullptr || !g_device_registry->isReady()) {
        Esp32BaseWeb::sendJson(503, "{\"ok\":false,\"error\":\"registry_unavailable\"}");
        return;
    }
    if (g_action_runtime != nullptr && g_action_runtime->isBusy()) {
        Esp32BaseWeb::sendJson(409, "{\"ok\":false,\"error\":\"action_busy\"}");
        return;
    }

    const uint8_t address = static_cast<uint8_t>(readUIntParam("address", 0u));
    const bool enabled = readUIntParam("enabled", 0u) != 0u;
    if (!fa_address_is_normal(address)) {
        Esp32BaseWeb::sendJson(400, "{\"ok\":false,\"error\":\"bad_address\"}");
        return;
    }
    if (!g_device_registry->setStationEnabled(address, enabled)) {
        Esp32BaseWeb::sendJson(404, "{\"ok\":false,\"error\":\"station_not_found\"}");
        return;
    }

    ESP32BASE_LOG_I("farm", "station_enabled_updated addr=%u enabled=%u",
                    address,
                    enabled ? 1u : 0u);
    Esp32BaseWeb::beginJson(200);
    Esp32BaseWeb::sendChunk("\"ok\":true,\"stationAddress\":");
    sendNumber(address);
    Esp32BaseWeb::sendChunk(",\"enabled\":");
    Esp32BaseWeb::sendChunk(enabled ? "true" : "false");
    Esp32BaseWeb::sendChunk(",\"message\":\"station state updated\"");
    Esp32BaseWeb::endJson();
}

void sendStationClearFaultApi(void) {
    if (!Esp32BaseWeb::checkPostAllowed("station_clear_fault")) {
        return;
    }
    if (g_rs485_master == nullptr || g_transport == nullptr) {
        Esp32BaseWeb::sendJson(503, "{\"ok\":false,\"error\":\"service_unavailable\"}");
        return;
    }
    if (g_action_runtime != nullptr && g_action_runtime->isBusy()) {
        Esp32BaseWeb::sendJson(409, "{\"ok\":false,\"error\":\"action_busy\"}");
        return;
    }
    if (!g_transport->isReady()) {
        ESP32BASE_LOG_W("farm", "station_clear_fault_blocked transport_not_configured");
        Esp32BaseWeb::sendJson(503, "{\"ok\":false,\"transport\":\"not_configured\"}");
        return;
    }

    const uint8_t address = static_cast<uint8_t>(readUIntParam("address", 0u));
    if (!fa_address_is_normal(address)) {
        Esp32BaseWeb::sendJson(400, "{\"ok\":false,\"error\":\"bad_address\"}");
        return;
    }

    uint8_t request[FA_MAX_FRAME_LEN];
    size_t request_len = 0u;
    uint8_t seq = 0u;
    const FaFrameResult frame_result = fa_rs485_master_build_clear_fault(g_rs485_master,
                                                                         address,
                                                                         request,
                                                                         sizeof(request),
                                                                         &request_len,
                                                                         &seq);
    if (frame_result != FA_FRAME_OK) {
        sendFeedTransportError(500, "clear_fault", "frame", frameResultName(frame_result));
        return;
    }

    FaMasterCommonResponse common;
    if (!transactAndParseCommon(address, seq, FA_CMD_CLEAR_FAULT, request, request_len, &common, "clear_fault")) {
        return;
    }
    if (g_device_registry != nullptr && g_device_registry->isReady()) {
        if (common.fault_code == FA_FAULT_NONE) {
            (void)g_device_registry->markStationOnline(address, FaMasterActionRuntime::nowSeconds());
        } else {
            (void)g_device_registry->markStationError(address, common.fault_code);
        }
    }

    ESP32BASE_LOG_I("farm", "station_clear_fault_accepted addr=%u state=%u fault=%u",
                    address,
                    common.station_state,
                    common.fault_code);
    Esp32BaseWeb::beginJson(200);
    Esp32BaseWeb::sendChunk("\"ok\":true,\"command\":\"clear_fault\",\"stationAddress\":");
    sendNumber(address);
    Esp32BaseWeb::sendChunk(",\"stationState\":");
    sendNumber(common.station_state);
    Esp32BaseWeb::sendChunk(",\"faultCode\":");
    sendNumber(common.fault_code);
    Esp32BaseWeb::sendChunk(",\"message\":\"clear fault accepted by station\"");
    Esp32BaseWeb::endJson();
}
