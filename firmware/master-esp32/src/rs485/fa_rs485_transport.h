#ifndef FA_RS485_TRANSPORT_H
#define FA_RS485_TRANSPORT_H

#include "fa_protocol.h"
#include "fa_station_node.h"

#include <stddef.h>
#include <stdint.h>

namespace FaRs485Config {
constexpr const char* NS = "fa_rs485";
constexpr const char* KEY_MODE = "mode";
constexpr const char* KEY_UART = "uart";
constexpr const char* KEY_RX_PIN = "rx";
constexpr const char* KEY_TX_PIN = "tx";
constexpr const char* KEY_DE_PIN = "de";
constexpr const char* KEY_BAUD = "baud";
constexpr const char* KEY_TIMEOUT_MS = "timeout";
}  // namespace FaRs485Config

enum : uint8_t {
    FA_RS485_MODE_DISABLED = 0u,
    FA_RS485_MODE_REAL_UART = 1u,
    FA_RS485_MODE_SIMULATED = 2u
};

struct FaRs485TransportConfig {
    uint8_t mode;
    int8_t uart_num;
    int8_t rx_pin;
    int8_t tx_pin;
    int8_t de_pin;
    uint32_t baud;
    uint32_t timeout_ms;
};

enum class FaRs485TransportStatus : uint8_t {
    OK = 0,
    NOT_CONFIGURED,
    BAD_PARAM,
    WRITE_FAILED,
    BAD_FRAME,
    RESPONSE_TOO_LONG,
    TIMEOUT
};

class FaRs485Transport {
public:
    static FaRs485TransportConfig defaultConfig();
    static FaRs485TransportConfig readConfig();
    static const char* statusName(FaRs485TransportStatus status);
    static const char* modeName(uint8_t mode);

    bool begin(const FaRs485TransportConfig& config);
    bool isReady() const;
    bool isSimulated() const;
    const FaRs485TransportConfig& config() const;

    FaRs485TransportStatus transact(const uint8_t* request,
                                    size_t request_len,
                                    uint8_t* response,
                                    size_t response_cap,
                                    size_t* response_len,
                                    uint32_t timeout_ms = 0);

private:
    struct SimStationSlot {
        FaStationNode node = {};
        int32_t position_pulses = 0;
        uint32_t last_ms = 0u;
        bool initialized = false;
    };

    SimStationSlot* simStationByAddress(uint8_t address);
    void simStep(SimStationSlot& slot);
    FaRs485TransportStatus transactSimulated(const uint8_t* request,
                                             size_t request_len,
                                             uint8_t* response,
                                             size_t response_cap,
                                             size_t* response_len);

    FaRs485TransportConfig config_ = defaultConfig();
    SimStationSlot sim_stations_[2];
    void* serial_ = nullptr;
    bool ready_ = false;
};

#endif
