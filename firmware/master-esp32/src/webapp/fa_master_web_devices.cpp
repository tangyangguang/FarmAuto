#include "fa_master_web_internal.h"

namespace {

const char* deviceTypeName(uint8_t type) {
    switch (type) {
    case FA_DEVICE_TYPE_DOOR:
        return "door";
    case FA_DEVICE_TYPE_FEEDER:
        return "feeder";
    default:
        return "unknown";
    }
}

const char* stationStateName(uint8_t state) {
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
    sendNumber(device.station_id);
    Esp32BaseWeb::sendChunk(" / ");
    if (hasStation) {
        sendNumber(station.bus_address);
    } else {
        Esp32BaseWeb::sendChunk("-");
    }
    Esp32BaseWeb::sendChunk("</td><td>");
    Esp32BaseWeb::writeHtmlEscaped(device.enabled != 0u ? "enabled" : "disabled");
    Esp32BaseWeb::sendChunk("</td><td><form method='post' action='/api/devices/enabled' onsubmit='return once(this)'>");
    Esp32BaseWeb::sendChunk("<input type='hidden' name='deviceId' value='");
    sendNumber(device.device_id);
    Esp32BaseWeb::sendChunk("'><input type='hidden' name='enabled' value='");
    Esp32BaseWeb::sendChunk(device.enabled != 0u ? "0" : "1");
    Esp32BaseWeb::sendChunk("'><input type='submit' value='");
    Esp32BaseWeb::sendChunk(device.enabled != 0u ? "Disable" : "Enable");
    Esp32BaseWeb::sendChunk("'></form></td></tr>");
}

void sendStationRow(const FaStationRecord& station) {
    Esp32BaseWeb::sendChunk("<tr><td>");
    sendNumber(station.station_id);
    Esp32BaseWeb::sendChunk("</td><td>");
    sendNumber(station.bus_address);
    Esp32BaseWeb::sendChunk("</td><td>");
    Esp32BaseWeb::writeHtmlEscaped(station.enabled != 0u ? "enabled" : "disabled");
    Esp32BaseWeb::sendChunk("</td><td>");
    Esp32BaseWeb::writeHtmlEscaped(stationStateName(station.online_state));
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
    Esp32BaseWeb::sendChunk("</td></tr>");
}

}  // namespace

void sendDevicesPage(void) {
    if (!Esp32BaseWeb::checkAuth()) {
        return;
    }

    char value[24];
    Esp32BaseWeb::sendHeader("Devices");
    Esp32BaseWeb::sendPageTitle("Devices", "Business devices and their bound RS485 stations.");

    Esp32BaseWeb::beginMetricGrid();
    Esp32BaseWeb::sendMetric("Registry", g_device_registry != nullptr && g_device_registry->isReady() ? "ready" : "unavailable");
    snprintf(value, sizeof(value), "%u", g_device_registry != nullptr ? g_device_registry->deviceCount() : 0u);
    Esp32BaseWeb::sendMetric("Devices", value);
    snprintf(value, sizeof(value), "%u", g_device_registry != nullptr ? g_device_registry->stationCount() : 0u);
    Esp32BaseWeb::sendMetric("Stations", value);
    snprintf(value, sizeof(value), "%lu", static_cast<unsigned long>(g_device_registry != nullptr ? g_device_registry->sequence() : 0u));
    Esp32BaseWeb::sendMetric("Sequence", value);
    Esp32BaseWeb::endMetricGrid();

    if (g_device_registry == nullptr || !g_device_registry->isReady()) {
        Esp32BaseWeb::sendNotice(Esp32BaseWeb::UI_WARN, "Registry unavailable", "LittleFS device registry is not ready.");
        Esp32BaseWeb::sendFooter();
        return;
    }

    Esp32BaseWeb::beginPanel("Business devices");
    Esp32BaseWeb::sendChunk("<div class='tablewrap'><table class='evtable'><thead><tr><th>ID</th><th>Name</th><th>Type</th><th>No.</th><th>Station / Addr</th><th>State</th><th>Action</th></tr></thead><tbody>");
    for (uint8_t i = 0u; i < g_device_registry->deviceCount(); ++i) {
        FaDeviceRecord device;
        if (g_device_registry->deviceAt(i, device)) {
            sendDeviceRow(device);
        }
    }
    Esp32BaseWeb::sendChunk("</tbody></table></div>");
    Esp32BaseWeb::endPanel();

    Esp32BaseWeb::beginPanel("Stations");
    Esp32BaseWeb::sendChunk("<div class='tablewrap'><table class='evtable'><thead><tr><th>ID</th><th>Addr</th><th>Enabled</th><th>Online</th><th>Proto</th><th>FW</th><th>Caps</th><th>Seen</th><th>Error</th></tr></thead><tbody>");
    for (uint8_t i = 0u; i < g_device_registry->stationCount(); ++i) {
        FaStationRecord station;
        if (g_device_registry->stationAt(i, station)) {
            sendStationRow(station);
        }
    }
    Esp32BaseWeb::sendChunk("</tbody></table></div>");
    Esp32BaseWeb::endPanel();

    Esp32BaseWeb::sendInfoRowCompactLink("Bus discovery", "Run RS485 scan to refresh station firmware, protocol and capability information.", "RS485", "/bus", "Open", Esp32BaseWeb::UI_INFO);
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
