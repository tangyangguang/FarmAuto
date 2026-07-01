#include "fa_rs485_transport.h"

#include <Arduino.h>
#include <Esp32Base.h>
#include <string.h>

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
    if (config.mode == FA_RS485_MODE_SIMULATED) {
        return true;
    }
    if (config.mode == FA_RS485_MODE_DISABLED) {
        return false;
    }
    if (config.mode != FA_RS485_MODE_REAL_UART) {
        return false;
    }
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
    config.mode = FA_RS485_MODE_SIMULATED;
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
    config.mode = static_cast<uint8_t>(Esp32BaseConfig::getInt(FaRs485Config::NS, FaRs485Config::KEY_MODE, config.mode));
    config.uart_num = static_cast<int8_t>(Esp32BaseConfig::getInt(FaRs485Config::NS, FaRs485Config::KEY_UART, config.uart_num));
    config.rx_pin = static_cast<int8_t>(Esp32BaseConfig::getInt(FaRs485Config::NS, FaRs485Config::KEY_RX_PIN, config.rx_pin));
    config.tx_pin = static_cast<int8_t>(Esp32BaseConfig::getInt(FaRs485Config::NS, FaRs485Config::KEY_TX_PIN, config.tx_pin));
    config.de_pin = static_cast<int8_t>(Esp32BaseConfig::getInt(FaRs485Config::NS, FaRs485Config::KEY_DE_PIN, config.de_pin));
    config.baud = static_cast<uint32_t>(Esp32BaseConfig::getInt(FaRs485Config::NS, FaRs485Config::KEY_BAUD, config.baud));
    config.timeout_ms = static_cast<uint32_t>(Esp32BaseConfig::getInt(FaRs485Config::NS, FaRs485Config::KEY_TIMEOUT_MS, config.timeout_ms));
    return config;
}

const char* FaRs485Transport::modeName(uint8_t mode) {
    switch (mode) {
    case FA_RS485_MODE_DISABLED:
        return "disabled";
    case FA_RS485_MODE_REAL_UART:
        return "real_uart";
    case FA_RS485_MODE_SIMULATED:
        return "simulated";
    default:
        return "unknown";
    }
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
    memset(sim_stations_, 0, sizeof(sim_stations_));

    if (!validConfig(config_)) {
        return false;
    }
    if (config_.mode == FA_RS485_MODE_SIMULATED) {
        ESP32BASE_LOG_W("farm", "rs485_transport_simulated stations=1,2");
        ready_ = true;
        return true;
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

bool FaRs485Transport::isSimulated() const {
    return ready_ && config_.mode == FA_RS485_MODE_SIMULATED;
}

const FaRs485TransportConfig& FaRs485Transport::config() const {
    return config_;
}

FaRs485Transport::SimStationSlot* FaRs485Transport::simStationByAddress(uint8_t address) {
    if (address != 1u && address != 2u) {
        return nullptr;
    }
    SimStationSlot& slot = sim_stations_[address - 1u];
    if (!slot.initialized) {
        fa_station_node_init(&slot.node, address);
        slot.position_pulses = 0;
        slot.last_ms = millis();
        slot.initialized = true;
    }
    return &slot;
}

void FaRs485Transport::simStep(SimStationSlot& slot) {
    const uint32_t now_ms = millis();
    const uint32_t delta_ms = now_ms - slot.last_ms;
    slot.last_ms = now_ms;
    if (slot.node.output.motor_enable != 0u) {
        const int32_t delta = static_cast<int32_t>(delta_ms * 3u);
        slot.position_pulses += slot.node.output.direction >= 0 ? delta : -delta;
    }
    fa_station_node_tick(&slot.node, now_ms, slot.position_pulses, 300u);
}

FaRs485TransportStatus FaRs485Transport::transactSimulated(const uint8_t* request,
                                                           size_t request_len,
                                                           uint8_t* response,
                                                           size_t response_cap,
                                                           size_t* response_len) {
    if (request == nullptr || request_len == 0u || request_len > FA_MAX_FRAME_LEN ||
        response == nullptr || response_len == nullptr || response_cap < FA_FRAME_OVERHEAD) {
        return FaRs485TransportStatus::BAD_PARAM;
    }

    FaFrame request_frame;
    const FaFrameResult decode_result = fa_frame_decode(request, request_len, &request_frame);
    if (decode_result != FA_FRAME_OK) {
        return FaRs485TransportStatus::BAD_FRAME;
    }

    SimStationSlot* slot = simStationByAddress(request_frame.dst);
    if (slot == nullptr) {
        return FaRs485TransportStatus::TIMEOUT;
    }
    simStep(*slot);
    const FaFrameResult handle_result = fa_station_node_handle_frame(&slot->node,
                                                                     &request_frame,
                                                                     response,
                                                                     response_cap,
                                                                     response_len);
    simStep(*slot);
    if (handle_result == FA_FRAME_ERR_OUTPUT_TOO_SMALL) {
        return FaRs485TransportStatus::RESPONSE_TOO_LONG;
    }
    if (handle_result != FA_FRAME_OK) {
        return FaRs485TransportStatus::BAD_FRAME;
    }
    ESP32BASE_LOG_I("farm", "rs485_sim addr=%u cmd=0x%02x len=%u",
                    request_frame.dst,
                    request_frame.cmd,
                    static_cast<unsigned>(request_len));
    return FaRs485TransportStatus::OK;
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
        if (ready_ && config_.mode == FA_RS485_MODE_SIMULATED) {
            return transactSimulated(request, request_len, response, response_cap, response_len);
        }
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
