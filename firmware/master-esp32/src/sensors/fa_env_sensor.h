#ifndef FA_ENV_SENSOR_H
#define FA_ENV_SENSOR_H

#include <stdint.h>

namespace FaEnvSensorConfig {
constexpr const char* NS = "fa_env";
constexpr const char* KEY_ENABLED = "enabled";
constexpr const char* KEY_SDA_PIN = "sda";
constexpr const char* KEY_SCL_PIN = "scl";
constexpr const char* KEY_ADDRESS = "addr";
constexpr const char* KEY_INTERVAL_MS = "interval";
constexpr const char* KEY_RECORD_INTERVAL_S = "rec_s";
}

struct FaEnvSensorSnapshot {
    bool enabled = false;
    bool ready = false;
    bool valid = false;
    int8_t sda_pin = -1;
    int8_t scl_pin = -1;
    uint8_t address = 0x44u;
    uint32_t interval_ms = 5000u;
    uint32_t record_interval_s = 300u;
    int32_t temperature_c_x100 = 0;
    uint32_t humidity_x100 = 0u;
    uint32_t sampled_at_s = 0u;
    uint32_t sampled_uptime_ms = 0u;
    uint32_t success_count = 0u;
    uint32_t fail_count = 0u;
    char last_error[24] = {};
};

class FaEnvSensorService {
public:
    void begin();
    void handle();
    bool readNow();
    FaEnvSensorSnapshot snapshot() const;

private:
    struct Config {
        bool enabled = true;
        int8_t sda_pin = 21;
        int8_t scl_pin = 22;
        uint8_t address = 0x44u;
        uint32_t interval_ms = 5000u;
        uint32_t record_interval_s = 300u;
    };

    Config readConfig() const;
    bool ensureBus(const Config& config);
    bool readSht30(const Config& config, int32_t& temperature_c_x100, uint32_t& humidity_x100);
    void setError(const char* error);
    void appendSampleEvent();

    FaEnvSensorSnapshot state_ = {};
    bool bus_ready_ = false;
    int8_t active_sda_ = -1;
    int8_t active_scl_ = -1;
    uint8_t active_address_ = 0u;
    uint32_t last_read_ms_ = 0u;
    uint32_t last_record_s_ = 0u;
};

#endif
