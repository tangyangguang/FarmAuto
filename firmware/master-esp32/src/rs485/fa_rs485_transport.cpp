#include "fa_rs485_transport.h"

#include <Arduino.h>
#include <Esp32Base.h>

namespace {

constexpr uint32_t kDefaultBaud = 115200u;
constexpr uint32_t kDefaultTimeoutMs = 80u;
constexpr uint32_t kTurnaroundDelayUs = 50u;

bool validPin(int8_t pin) {
    return pin >= 0 && pin <= 39;
}

bool validOutputPin(int8_t pin) {
    return pin >= 0 && pin <= 33;
}

bool validConfig(const FaRs485TransportConfig& config) {
    return (config.uart_num == 1 || config.uart_num == 2) &&
           validPin(config.rx_pin) &&
           validOutputPin(config.tx_pin) &&
           validOutputPin(config.de_pin) &&
           config.baud >= 9600u &&
           config.baud <= 1000000u &&
           config.timeout_ms >= 20u &&
           config.timeout_ms <= 2000u;
}

HardwareSerial* serialFor(int8_t uart_num) {
    if (uart_num == 1) {
        return &Serial1;
    }
    if (uart_num == 2) {
        return &Serial2;
    }
    return nullptr;
}

}  // namespace

FaRs485TransportConfig FaRs485Transport::defaultConfig() {
    FaRs485TransportConfig config = {};
    config.uart_num = 2;
    config.rx_pin = -1;
    config.tx_pin = -1;
    config.de_pin = -1;
    config.baud = kDefaultBaud;
    config.timeout_ms = kDefaultTimeoutMs;
    return config;
}

FaRs485TransportConfig FaRs485Transport::readConfig() {
    FaRs485TransportConfig config = defaultConfig();
    config.uart_num = static_cast<int8_t>(Esp32BaseConfig::getInt(FaRs485Config::NS, FaRs485Config::KEY_UART, config.uart_num));
    config.rx_pin = static_cast<int8_t>(Esp32BaseConfig::getInt(FaRs485Config::NS, FaRs485Config::KEY_RX_PIN, config.rx_pin));
    config.tx_pin = static_cast<int8_t>(Esp32BaseConfig::getInt(FaRs485Config::NS, FaRs485Config::KEY_TX_PIN, config.tx_pin));
    config.de_pin = static_cast<int8_t>(Esp32BaseConfig::getInt(FaRs485Config::NS, FaRs485Config::KEY_DE_PIN, config.de_pin));
    config.baud = static_cast<uint32_t>(Esp32BaseConfig::getInt(FaRs485Config::NS, FaRs485Config::KEY_BAUD, config.baud));
    config.timeout_ms = static_cast<uint32_t>(Esp32BaseConfig::getInt(FaRs485Config::NS, FaRs485Config::KEY_TIMEOUT_MS, config.timeout_ms));
    return config;
}

const char* FaRs485Transport::statusName(FaRs485TransportStatus status) {
    switch (status) {
    case FaRs485TransportStatus::OK:
        return "ok";
    case FaRs485TransportStatus::NOT_CONFIGURED:
        return "not_configured";
    case FaRs485TransportStatus::BAD_PARAM:
        return "bad_param";
    case FaRs485TransportStatus::WRITE_FAILED:
        return "write_failed";
    case FaRs485TransportStatus::BAD_FRAME:
        return "bad_frame";
    case FaRs485TransportStatus::RESPONSE_TOO_LONG:
        return "response_too_long";
    case FaRs485TransportStatus::TIMEOUT:
        return "timeout";
    default:
        return "unknown";
    }
}

bool FaRs485Transport::begin(const FaRs485TransportConfig& config) {
    ready_ = false;
    serial_ = nullptr;
    config_ = config;

    if (!validConfig(config_)) {
        return false;
    }

    HardwareSerial* serial = serialFor(config_.uart_num);
    if (serial == nullptr) {
        return false;
    }

    pinMode(config_.de_pin, OUTPUT);
    digitalWrite(config_.de_pin, LOW);
    serial->begin(config_.baud, SERIAL_8N1, config_.rx_pin, config_.tx_pin);
    while (serial->available() > 0) {
        (void)serial->read();
    }

    serial_ = serial;
    ready_ = true;
    return true;
}

bool FaRs485Transport::isReady() const {
    return ready_;
}

const FaRs485TransportConfig& FaRs485Transport::config() const {
    return config_;
}

FaRs485TransportStatus FaRs485Transport::transact(const uint8_t* request,
                                                  size_t request_len,
                                                  uint8_t* response,
                                                  size_t response_cap,
                                                  size_t* response_len,
                                                  uint32_t timeout_ms) {
    if (response_len != nullptr) {
        *response_len = 0u;
    }
    if (!ready_ || serial_ == nullptr) {
        return FaRs485TransportStatus::NOT_CONFIGURED;
    }
    if (request == nullptr || request_len == 0u || request_len > FA_MAX_FRAME_LEN ||
        response == nullptr || response_len == nullptr || response_cap < FA_FRAME_OVERHEAD) {
        return FaRs485TransportStatus::BAD_PARAM;
    }

    HardwareSerial* serial = static_cast<HardwareSerial*>(serial_);
    while (serial->available() > 0) {
        (void)serial->read();
    }

    digitalWrite(config_.de_pin, HIGH);
    delayMicroseconds(kTurnaroundDelayUs);
    const size_t written = serial->write(request, request_len);
    serial->flush();
    delayMicroseconds(kTurnaroundDelayUs);
    digitalWrite(config_.de_pin, LOW);
    if (written != request_len) {
        return FaRs485TransportStatus::WRITE_FAILED;
    }

    FaFrameParser parser;
    FaFrame frame;
    fa_frame_parser_init(&parser);

    const uint32_t deadline = millis() + (timeout_ms == 0u ? config_.timeout_ms : timeout_ms);
    while ((int32_t)(millis() - deadline) < 0) {
        while (serial->available() > 0) {
            const int raw = serial->read();
            if (raw < 0) {
                continue;
            }
            const FaFrameResult result = fa_frame_parser_push(&parser, static_cast<uint8_t>(raw), &frame);
            if (result == FA_FRAME_INCOMPLETE) {
                continue;
            }
            if (result != FA_FRAME_OK) {
                return FaRs485TransportStatus::BAD_FRAME;
            }

            const FaFrameResult encode_result = fa_frame_encode(&frame, response, response_cap, response_len);
            if (encode_result == FA_FRAME_ERR_OUTPUT_TOO_SMALL) {
                return FaRs485TransportStatus::RESPONSE_TOO_LONG;
            }
            if (encode_result != FA_FRAME_OK) {
                return FaRs485TransportStatus::BAD_FRAME;
            }
            return FaRs485TransportStatus::OK;
        }
        yield();
    }

    return FaRs485TransportStatus::TIMEOUT;
}
