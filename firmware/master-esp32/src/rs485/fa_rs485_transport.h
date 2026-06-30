#ifndef FA_RS485_TRANSPORT_H
#define FA_RS485_TRANSPORT_H

#include "fa_protocol.h"

#include <stddef.h>
#include <stdint.h>

namespace FaRs485Config {
constexpr const char* NS = "fa_rs485";
constexpr const char* KEY_UART = "uart";
constexpr const char* KEY_RX_PIN = "rx";
constexpr const char* KEY_TX_PIN = "tx";
constexpr const char* KEY_DE_PIN = "de";
constexpr const char* KEY_BAUD = "baud";
constexpr const char* KEY_TIMEOUT_MS = "timeout";
}  // namespace FaRs485Config

struct FaRs485TransportConfig {
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

    bool begin(const FaRs485TransportConfig& config);
    bool isReady() const;
    const FaRs485TransportConfig& config() const;

    FaRs485TransportStatus transact(const uint8_t* request,
                                    size_t request_len,
                                    uint8_t* response,
                                    size_t response_cap,
                                    size_t* response_len,
                                    uint32_t timeout_ms = 0);

private:
    FaRs485TransportConfig config_ = defaultConfig();
    void* serial_ = nullptr;
    bool ready_ = false;
};

#endif
