#include "fa_master_web_internal.h"

namespace {

constexpr uint16_t kDefaultScanTimeoutMs = 25u;

struct FaBusScanNode {
    uint8_t address;
    uint8_t effective_address;
    uint8_t raw_address;
    uint8_t device_class;
    uint16_t firmware_version;
    uint32_t capability_flags;
    uint8_t max_payload_len;
};

uint8_t clampAddress(uint32_t value, uint8_t fallback) {
    if (value < FA_ADDRESS_MIN || value > FA_ADDRESS_MAX) {
        return fallback;
    }
    return static_cast<uint8_t>(value);
}

uint16_t clampScanTimeout(uint32_t value) {
    if (value < 20u) {
        return 20u;
    }
    if (value > 2000u) {
        return 2000u;
    }
    return static_cast<uint16_t>(value);
}

void sendBusMetricPins(void) {
    char pins[40];
    if (g_transport == nullptr) {
        Esp32BaseWeb::sendMetric("Pins", "-");
        return;
    }
    const FaRs485TransportConfig& config = g_transport->config();
    snprintf(pins, sizeof(pins), "RX %d / TX %d / DE %d", config.rx_pin, config.tx_pin, config.de_pin);
    Esp32BaseWeb::sendMetric("Pins", pins);
}

void sendBusNodeJson(const FaBusScanNode& node) {
    Esp32BaseWeb::sendChunk("{\"address\":");
    sendNumber(node.address);
    Esp32BaseWeb::sendChunk(",\"effectiveAddress\":");
    sendNumber(node.effective_address);
    Esp32BaseWeb::sendChunk(",\"rawAddress\":");
    sendNumber(node.raw_address);
    Esp32BaseWeb::sendChunk(",\"deviceClass\":");
    sendNumber(node.device_class);
    Esp32BaseWeb::sendChunk(",\"firmwareVersion\":");
    sendNumber(node.firmware_version);
    Esp32BaseWeb::sendChunk(",\"capabilityFlags\":");
    sendNumber(node.capability_flags);
    Esp32BaseWeb::sendChunk(",\"maxPayloadLen\":");
    sendNumber(node.max_payload_len);
    Esp32BaseWeb::sendChunk("}");
}

}  // namespace

void sendBusPage(void) {
    if (!Esp32BaseWeb::checkAuth()) {
        return;
    }

    char value[24];
    const FaRs485TransportConfig config = g_transport != nullptr ? g_transport->config() : FaRs485Transport::defaultConfig();

    Esp32BaseWeb::sendHeader("RS485 Bus");
    Esp32BaseWeb::sendPageTitle("Bus scan", "Pings station addresses and reports nodes that answer.");

    Esp32BaseWeb::beginMetricGrid();
    Esp32BaseWeb::sendMetric("State", g_transport != nullptr && g_transport->isReady() ? "ready" : "not configured");
    snprintf(value, sizeof(value), "%lu", static_cast<unsigned long>(config.baud));
    Esp32BaseWeb::sendMetric("Baud", value, "bps");
    snprintf(value, sizeof(value), "%lu", static_cast<unsigned long>(config.timeout_ms));
    Esp32BaseWeb::sendMetric("Default timeout", value, "ms");
    sendBusMetricPins();
    Esp32BaseWeb::endMetricGrid();

    Esp32BaseWeb::beginPanel("Scan");
    Esp32BaseWeb::sendChunk("<form method='post' action='/api/bus/scan' onsubmit='return once(this)'><div class='fieldgrid'>");
    Esp32BaseWeb::sendChunk("<div class='field med'><label>Start address</label><input type='number' name='start' min='1' max='127' value='1'></div>");
    Esp32BaseWeb::sendChunk("<div class='field med'><label>End address</label><input type='number' name='end' min='1' max='127' value='127'></div>");
    Esp32BaseWeb::sendChunk("<div class='field med'><label>Timeout</label><input type='number' name='timeout' min='20' max='2000' value='25'><small>Per address, ms.</small></div>");
    Esp32BaseWeb::sendChunk("</div><div class='actions'><input type='submit' value='Scan'></div></form>");
    Esp32BaseWeb::endPanel();

    Esp32BaseWeb::sendInfoRowCompactLink("RS485 parameters", "UART, pins, baud and default timeout are stored by Esp32Base App Config.", "App Config", "/esp32base/app-config", "Edit", Esp32BaseWeb::UI_INFO);
    Esp32BaseWeb::sendFooter();
}

void sendBusScanApi(void) {
    if (!Esp32BaseWeb::checkPostAllowed("bus_scan")) {
        return;
    }
    if (g_rs485_master == nullptr || g_transport == nullptr) {
        Esp32BaseWeb::sendJson(503, "{\"ok\":false,\"error\":\"service_unavailable\"}");
        return;
    }
    if (!g_transport->isReady()) {
        Esp32BaseWeb::sendJson(503, "{\"ok\":false,\"transport\":\"not_configured\"}");
        return;
    }

    const uint8_t start = clampAddress(readUIntParam("start", FA_ADDRESS_MIN), FA_ADDRESS_MIN);
    const uint8_t end = clampAddress(readUIntParam("end", FA_ADDRESS_MAX), FA_ADDRESS_MAX);
    const uint16_t timeout_ms = clampScanTimeout(readUIntParam("timeout", kDefaultScanTimeoutMs));
    if (start > end) {
        Esp32BaseWeb::sendJson(400, "{\"ok\":false,\"error\":\"bad_range\"}");
        return;
    }

    FaBusScanNode nodes[FA_ADDRESS_MAX];
    uint8_t found = 0u;
    uint16_t timeouts = 0u;
    uint16_t errors = 0u;
    uint8_t request[FA_MAX_FRAME_LEN];
    uint8_t response[FA_MAX_FRAME_LEN];

    for (uint8_t address = start; address <= end; ++address) {
        size_t request_len = 0u;
        size_t response_len = 0u;
        uint8_t seq = 0u;
        const FaFrameResult frame_result = fa_rs485_master_build_ping(g_rs485_master,
                                                                      address,
                                                                      request,
                                                                      sizeof(request),
                                                                      &request_len,
                                                                      &seq);
        if (frame_result != FA_FRAME_OK) {
            ++errors;
        } else {
            const FaRs485TransportStatus tx_status = g_transport->transact(request,
                                                                           request_len,
                                                                           response,
                                                                           sizeof(response),
                                                                           &response_len,
                                                                           timeout_ms);
            if (tx_status == FaRs485TransportStatus::TIMEOUT) {
                ++timeouts;
            } else if (tx_status != FaRs485TransportStatus::OK) {
                ++errors;
            } else {
                FaMasterPingResponse ping;
                const uint8_t status = fa_rs485_master_parse_ping(response, response_len, address, seq, &ping);
                if (status == FA_STATUS_OK && found < FA_ADDRESS_MAX) {
                    nodes[found].address = address;
                    nodes[found].effective_address = ping.effective_bus_address;
                    nodes[found].raw_address = ping.raw_address_input;
                    nodes[found].device_class = ping.device_class;
                    nodes[found].firmware_version = ping.firmware_version;
                    nodes[found].capability_flags = ping.capability_flags;
                    nodes[found].max_payload_len = ping.max_payload_len;
                    ++found;
                } else {
                    ++errors;
                }
            }
        }
        yield();
    }

    Esp32BaseWeb::beginJson(200);
    Esp32BaseWeb::sendChunk("\"ok\":true,\"start\":");
    sendNumber(start);
    Esp32BaseWeb::sendChunk(",\"end\":");
    sendNumber(end);
    Esp32BaseWeb::sendChunk(",\"timeoutMs\":");
    sendNumber(timeout_ms);
    Esp32BaseWeb::sendChunk(",\"scanned\":");
    sendNumber(static_cast<uint32_t>(end - start + 1u));
    Esp32BaseWeb::sendChunk(",\"found\":");
    sendNumber(found);
    Esp32BaseWeb::sendChunk(",\"timeouts\":");
    sendNumber(timeouts);
    Esp32BaseWeb::sendChunk(",\"errors\":");
    sendNumber(errors);
    Esp32BaseWeb::sendChunk(",\"nodes\":[");
    for (uint8_t i = 0u; i < found; ++i) {
        if (i != 0u) {
            Esp32BaseWeb::sendChunk(",");
        }
        sendBusNodeJson(nodes[i]);
    }
    Esp32BaseWeb::sendChunk("]");
    Esp32BaseWeb::endJson();
}
