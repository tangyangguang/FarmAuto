#include "fa_env_sensor.h"

#include <Arduino.h>
#include <Esp32Base.h>
#include <Wire.h>
#include "runtime/Esp32BaseAppEventLog.h"
#include <string.h>

namespace {

uint8_t shtCrc8(const uint8_t* data, size_t len) {
    uint8_t crc = 0xFFu;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (uint8_t bit = 0u; bit < 8u; ++bit) {
            crc = (crc & 0x80u) != 0u ? static_cast<uint8_t>((crc << 1u) ^ 0x31u) : static_cast<uint8_t>(crc << 1u);
        }
    }
    return crc;
}

uint32_t clampU32(int32_t value, uint32_t min_value, uint32_t max_value, uint32_t fallback) {
    if (value < static_cast<int32_t>(min_value) || value > static_cast<int32_t>(max_value)) {
        return fallback;
    }
    return static_cast<uint32_t>(value);
}

int8_t clampPin(int32_t value, int8_t fallback) {
    if (value < -1 || value > 39) {
        return fallback;
    }
    return static_cast<int8_t>(value);
}

uint32_t nowSeconds() {
    const Esp32BaseTime::Snapshot now = Esp32BaseTime::snapshot();
    return now.synced ? now.epochSec : now.uptimeSec;
}

}  // namespace

void FaEnvSensorService::begin() {
    state_ = {};
    const Config config = readConfig();
    state_.enabled = config.enabled;
    state_.sda_pin = config.sda_pin;
    state_.scl_pin = config.scl_pin;
    state_.address = config.address;
    state_.interval_ms = config.interval_ms;
    state_.record_interval_s = config.record_interval_s;
    setError(config.enabled ? "not_read" : "disabled");
}

void FaEnvSensorService::handle() {
    const Config config = readConfig();
    state_.enabled = config.enabled;
    state_.sda_pin = config.sda_pin;
    state_.scl_pin = config.scl_pin;
    state_.address = config.address;
    state_.interval_ms = config.interval_ms;
    state_.record_interval_s = config.record_interval_s;
    if (!config.enabled) {
        state_.ready = false;
        setError("disabled");
        return;
    }

    const uint32_t now_ms = millis();
    if (last_read_ms_ != 0u && now_ms - last_read_ms_ < config.interval_ms) {
        return;
    }
    last_read_ms_ = now_ms;
    (void)readNow();
}

bool FaEnvSensorService::readNow() {
    const Config config = readConfig();
    state_.enabled = config.enabled;
    state_.sda_pin = config.sda_pin;
    state_.scl_pin = config.scl_pin;
    state_.address = config.address;
    state_.interval_ms = config.interval_ms;
    state_.record_interval_s = config.record_interval_s;
    last_read_ms_ = millis();
    if (!config.enabled) {
        state_.ready = false;
        state_.valid = false;
        setError("disabled");
        return false;
    }
    if (!ensureBus(config)) {
        state_.ready = false;
        state_.valid = false;
        ++state_.fail_count;
        return false;
    }

    int32_t temperature = 0;
    uint32_t humidity = 0u;
    if (!readSht30(config, temperature, humidity)) {
        state_.ready = true;
        state_.valid = false;
        ++state_.fail_count;
        return false;
    }

    state_.ready = true;
    state_.valid = true;
    state_.temperature_c_x100 = temperature;
    state_.humidity_x100 = humidity;
    state_.sampled_at_s = nowSeconds();
    state_.sampled_uptime_ms = millis();
    ++state_.success_count;
    setError("ok");
    ESP32BASE_LOG_I("farm", "env_sample temp_c_x100=%ld humidity_x100=%lu addr=0x%02x",
                    static_cast<long>(temperature),
                    static_cast<unsigned long>(humidity),
                    config.address);
    appendSampleEvent();
    return true;
}

FaEnvSensorSnapshot FaEnvSensorService::snapshot() const {
    return state_;
}

FaEnvSensorService::Config FaEnvSensorService::readConfig() const {
    Config config;
    config.enabled = Esp32BaseConfig::getBool(FaEnvSensorConfig::NS, FaEnvSensorConfig::KEY_ENABLED, true);
    config.sda_pin = clampPin(Esp32BaseConfig::getInt(FaEnvSensorConfig::NS, FaEnvSensorConfig::KEY_SDA_PIN, 21), 21);
    config.scl_pin = clampPin(Esp32BaseConfig::getInt(FaEnvSensorConfig::NS, FaEnvSensorConfig::KEY_SCL_PIN, 22), 22);
    config.address = static_cast<uint8_t>(clampU32(Esp32BaseConfig::getInt(FaEnvSensorConfig::NS, FaEnvSensorConfig::KEY_ADDRESS, 0x44), 0x08u, 0x77u, 0x44u));
    config.interval_ms = clampU32(Esp32BaseConfig::getInt(FaEnvSensorConfig::NS, FaEnvSensorConfig::KEY_INTERVAL_MS, 5000), 1000u, 600000u, 5000u);
    config.record_interval_s = clampU32(Esp32BaseConfig::getInt(FaEnvSensorConfig::NS, FaEnvSensorConfig::KEY_RECORD_INTERVAL_S, 300), 10u, 86400u, 300u);
    return config;
}

bool FaEnvSensorService::ensureBus(const Config& config) {
    if (config.sda_pin < 0 || config.scl_pin < 0) {
        bus_ready_ = false;
        setError("pin_disabled");
        return false;
    }
    if (bus_ready_ &&
        active_sda_ == config.sda_pin &&
        active_scl_ == config.scl_pin &&
        active_address_ == config.address) {
        return true;
    }
    Wire.end();
    if (!Wire.begin(config.sda_pin, config.scl_pin)) {
        bus_ready_ = false;
        setError("wire_begin");
        ESP32BASE_LOG_W("farm", "env_i2c_begin_failed sda=%d scl=%d", config.sda_pin, config.scl_pin);
        return false;
    }
    Wire.setClock(100000u);
    active_sda_ = config.sda_pin;
    active_scl_ = config.scl_pin;
    active_address_ = config.address;
    bus_ready_ = true;
    ESP32BASE_LOG_I("farm", "env_i2c_ready sda=%d scl=%d addr=0x%02x", config.sda_pin, config.scl_pin, config.address);
    return true;
}

bool FaEnvSensorService::readSht30(const Config& config, int32_t& temperature_c_x100, uint32_t& humidity_x100) {
    Wire.beginTransmission(config.address);
    Wire.write(0x24u);
    Wire.write(0x00u);
    if (Wire.endTransmission() != 0) {
        setError("i2c_write");
        ESP32BASE_LOG_W("farm", "env_read_failed stage=write addr=0x%02x", config.address);
        return false;
    }

    delay(16);
    const uint8_t expected = 6u;
    const uint8_t got = Wire.requestFrom(static_cast<int>(config.address), static_cast<int>(expected));
    if (got != expected) {
        setError("i2c_read");
        ESP32BASE_LOG_W("farm", "env_read_failed stage=read addr=0x%02x got=%u", config.address, got);
        while (Wire.available() > 0) {
            (void)Wire.read();
        }
        return false;
    }

    uint8_t data[6];
    for (uint8_t i = 0u; i < sizeof(data); ++i) {
        data[i] = static_cast<uint8_t>(Wire.read());
    }
    if (shtCrc8(data, 2u) != data[2] || shtCrc8(data + 3u, 2u) != data[5]) {
        setError("crc");
        ESP32BASE_LOG_W("farm", "env_read_failed stage=crc addr=0x%02x", config.address);
        return false;
    }

    const uint16_t raw_t = static_cast<uint16_t>((static_cast<uint16_t>(data[0]) << 8) | data[1]);
    const uint16_t raw_h = static_cast<uint16_t>((static_cast<uint16_t>(data[3]) << 8) | data[4]);
    temperature_c_x100 = -4500 + static_cast<int32_t>((17500ul * raw_t + 32767ul) / 65535ul);
    humidity_x100 = static_cast<uint32_t>((10000ul * raw_h + 32767ul) / 65535ul);
    return true;
}

void FaEnvSensorService::setError(const char* error) {
    memset(state_.last_error, 0, sizeof(state_.last_error));
    if (error != nullptr) {
        strncpy(state_.last_error, error, sizeof(state_.last_error) - 1u);
    }
}

void FaEnvSensorService::appendSampleEvent() {
    if (state_.record_interval_s == 0u) {
        return;
    }
    const uint32_t now_s = nowSeconds();
    if (last_record_s_ != 0u && now_s - last_record_s_ < state_.record_interval_s) {
        return;
    }
    last_record_s_ = now_s;

    Esp32BaseAppEventLog::Event event;
    event.level = Esp32BaseAppEventLog::LEVEL_INFO;
    event.source = "farm";
    event.type = "env_sample";
    event.reason = "periodic";
    event.object = "sht30";
    event.value1 = state_.temperature_c_x100;
    event.value2 = static_cast<int32_t>(state_.humidity_x100);
    event.valueMask = Esp32BaseAppEventLog::VALUE1 | Esp32BaseAppEventLog::VALUE2;
    event.text = "SHT30 sample";
    if (!Esp32BaseAppEventLog::append(event)) {
        ESP32BASE_LOG_W("farm", "env_event_append_failed error=%s", Esp32BaseAppEventLog::lastError());
    }
}
