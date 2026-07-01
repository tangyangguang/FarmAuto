#include "fa_board_io.h"

#include <Arduino.h>
#include <Esp32Base.h>
#include <string.h>

namespace {

bool validInputPin(int8_t pin) {
    return pin >= 0 && pin <= 39;
}

bool validOutputPin(int8_t pin) {
    return pin >= 0 && pin <= 33;
}

int8_t clampPin(int32_t value, int8_t fallback) {
    if (value < -1 || value > 39) {
        return fallback;
    }
    return static_cast<int8_t>(value);
}

uint16_t clampU16(int32_t value, uint16_t min_value, uint16_t max_value, uint16_t fallback) {
    if (value < static_cast<int32_t>(min_value) || value > static_cast<int32_t>(max_value)) {
        return fallback;
    }
    return static_cast<uint16_t>(value);
}

bool sameConfig(const FaBoardIoService::Config& a, const FaBoardIoService::Config& b) {
    if (a.enabled != b.enabled ||
        a.led_active_low != b.led_active_low ||
        a.button_active_low != b.button_active_low ||
        a.run_led_pin != b.run_led_pin ||
        a.err_led_pin != b.err_led_pin ||
        a.debounce_ms != b.debounce_ms ||
        a.long_press_ms != b.long_press_ms) {
        return false;
    }
    for (uint8_t i = 0u; i < FA_BOARD_BUTTON_COUNT; ++i) {
        if (a.button_pins[i] != b.button_pins[i]) {
            return false;
        }
    }
    return true;
}

}  // namespace

void FaBoardIoService::begin() {
    state_ = {};
    memset(raw_button_pressed_, 0, sizeof(raw_button_pressed_));
    memset(stable_button_pressed_, 0, sizeof(stable_button_pressed_));
    memset(long_event_sent_, 0, sizeof(long_event_sent_));
    applyConfig(readConfig());
}

void FaBoardIoService::handle() {
    const Config config = readConfig();
    if (!configured_ || !sameConfig(config, active_config_)) {
        applyConfig(config);
    }
    state_.enabled = active_config_.enabled;
    ++state_.loop_count;
    if (!active_config_.enabled) {
        setLed(active_config_.run_led_pin, active_config_.led_active_low, false);
        setLed(active_config_.err_led_pin, active_config_.led_active_low, false);
        return;
    }

    const uint32_t now_ms = millis();
    updateLedOutputs(now_ms);
    updateButtons(now_ms);
}

FaBoardIoSnapshot FaBoardIoService::snapshot() const {
    return state_;
}

const char* FaBoardIoService::buttonRoleName(uint8_t role) {
    switch (role) {
    case FA_BOARD_BUTTON_BOOT:
        return "boot";
    case FA_BOARD_BUTTON_RESERVED:
        return "reserved";
    case FA_BOARD_BUTTON_OPEN:
        return "door_open";
    case FA_BOARD_BUTTON_CLOSE:
        return "door_close";
    case FA_BOARD_BUTTON_STOP:
        return "door_stop";
    default:
        return "unknown";
    }
}

FaBoardIoService::Config FaBoardIoService::readConfig() const {
    Config config;
    config.enabled = Esp32BaseConfig::getBool(FaBoardIoConfig::NS, FaBoardIoConfig::KEY_ENABLED, true);
    config.run_led_pin = clampPin(Esp32BaseConfig::getInt(FaBoardIoConfig::NS, FaBoardIoConfig::KEY_RUN_LED_PIN, 27), 27);
    config.err_led_pin = clampPin(Esp32BaseConfig::getInt(FaBoardIoConfig::NS, FaBoardIoConfig::KEY_ERR_LED_PIN, 14), 14);
    config.led_active_low = Esp32BaseConfig::getBool(FaBoardIoConfig::NS, FaBoardIoConfig::KEY_LED_ACTIVE_LOW, false);
    config.button_active_low = Esp32BaseConfig::getBool(FaBoardIoConfig::NS, FaBoardIoConfig::KEY_BUTTON_ACTIVE_LOW, true);
    config.button_pins[FA_BOARD_BUTTON_BOOT] = clampPin(Esp32BaseConfig::getInt(FaBoardIoConfig::NS, FaBoardIoConfig::KEY_BOOT_PIN, 0), 0);
    config.button_pins[FA_BOARD_BUTTON_RESERVED] = clampPin(Esp32BaseConfig::getInt(FaBoardIoConfig::NS, FaBoardIoConfig::KEY_BUTTON_1_PIN, -1), -1);
    config.button_pins[FA_BOARD_BUTTON_OPEN] = clampPin(Esp32BaseConfig::getInt(FaBoardIoConfig::NS, FaBoardIoConfig::KEY_BUTTON_2_PIN, -1), -1);
    config.button_pins[FA_BOARD_BUTTON_CLOSE] = clampPin(Esp32BaseConfig::getInt(FaBoardIoConfig::NS, FaBoardIoConfig::KEY_BUTTON_3_PIN, -1), -1);
    config.button_pins[FA_BOARD_BUTTON_STOP] = clampPin(Esp32BaseConfig::getInt(FaBoardIoConfig::NS, FaBoardIoConfig::KEY_BUTTON_4_PIN, -1), -1);
    config.debounce_ms = clampU16(Esp32BaseConfig::getInt(FaBoardIoConfig::NS, FaBoardIoConfig::KEY_DEBOUNCE_MS, 50), 10u, 1000u, 50u);
    config.long_press_ms = clampU16(Esp32BaseConfig::getInt(FaBoardIoConfig::NS, FaBoardIoConfig::KEY_LONG_PRESS_MS, 1000), 200u, 10000u, 1000u);
    return config;
}

void FaBoardIoService::applyConfig(const Config& config) {
    if (configured_) {
        setLed(active_config_.run_led_pin, active_config_.led_active_low, false);
        setLed(active_config_.err_led_pin, active_config_.led_active_low, false);
    }

    active_config_ = config;
    configured_ = true;
    state_.enabled = config.enabled;
    state_.led_active_low = config.led_active_low;
    state_.button_active_low = config.button_active_low;
    state_.run_led_pin = config.run_led_pin;
    state_.err_led_pin = config.err_led_pin;
    state_.debounce_ms = config.debounce_ms;
    state_.long_press_ms = config.long_press_ms;
    state_.run_led_on = false;
    state_.err_led_on = false;

    if (validOutputPin(config.run_led_pin)) {
        pinMode(config.run_led_pin, OUTPUT);
        setLed(config.run_led_pin, config.led_active_low, false);
    }
    if (validOutputPin(config.err_led_pin)) {
        pinMode(config.err_led_pin, OUTPUT);
        setLed(config.err_led_pin, config.led_active_low, false);
    }

    for (uint8_t i = 0u; i < FA_BOARD_BUTTON_COUNT; ++i) {
        FaBoardButtonSnapshot& button = state_.buttons[i];
        const uint32_t old_press_count = button.press_count;
        const uint32_t old_long_count = button.long_press_count;
        button = {};
        button.pin = config.button_pins[i];
        button.configured = validInputPin(config.button_pins[i]);
        button.press_count = old_press_count;
        button.long_press_count = old_long_count;
        raw_button_pressed_[i] = false;
        stable_button_pressed_[i] = false;
        long_event_sent_[i] = false;
        if (button.configured) {
            if (config.button_active_low && config.button_pins[i] <= 33) {
                pinMode(config.button_pins[i], INPUT_PULLUP);
            } else {
                pinMode(config.button_pins[i], INPUT);
            }
        }
    }
    snprintf(state_.last_event, sizeof(state_.last_event), "configured");
    ESP32BASE_LOG_I("farm", "board_io_configured run_led=%d err_led=%d boot=%d btn1=%d btn2=%d btn3=%d btn4=%d",
                    config.run_led_pin,
                    config.err_led_pin,
                    config.button_pins[FA_BOARD_BUTTON_BOOT],
                    config.button_pins[FA_BOARD_BUTTON_RESERVED],
                    config.button_pins[FA_BOARD_BUTTON_OPEN],
                    config.button_pins[FA_BOARD_BUTTON_CLOSE],
                    config.button_pins[FA_BOARD_BUTTON_STOP]);
}

void FaBoardIoService::updateLedOutputs(uint32_t now_ms) {
    const bool run_on = (now_ms / 500u) % 2u == 0u;
    const bool err_on = false;
    state_.run_led_on = run_on;
    state_.err_led_on = err_on;
    setLed(active_config_.run_led_pin, active_config_.led_active_low, run_on);
    setLed(active_config_.err_led_pin, active_config_.led_active_low, err_on);
}

void FaBoardIoService::updateButtons(uint32_t now_ms) {
    for (uint8_t i = 0u; i < FA_BOARD_BUTTON_COUNT; ++i) {
        FaBoardButtonSnapshot& button = state_.buttons[i];
        if (!button.configured) {
            continue;
        }
        const int value = digitalRead(button.pin);
        const bool raw_pressed = active_config_.button_active_low ? value == LOW : value == HIGH;
        if (raw_pressed != raw_button_pressed_[i]) {
            raw_button_pressed_[i] = raw_pressed;
            button.last_change_ms = now_ms;
        }
        if (raw_pressed != stable_button_pressed_[i] &&
            static_cast<uint32_t>(now_ms - button.last_change_ms) >= active_config_.debounce_ms) {
            stable_button_pressed_[i] = raw_pressed;
            button.pressed = raw_pressed;
            if (raw_pressed) {
                button.press_started_ms = now_ms;
                ++button.press_count;
                long_event_sent_[i] = false;
                recordButtonEvent(i, false);
            } else {
                button.long_pressed = false;
            }
        }
        if (stable_button_pressed_[i] &&
            !long_event_sent_[i] &&
            static_cast<uint32_t>(now_ms - button.press_started_ms) >= active_config_.long_press_ms) {
            long_event_sent_[i] = true;
            button.long_pressed = true;
            ++button.long_press_count;
            recordButtonEvent(i, true);
        }
    }
}

void FaBoardIoService::setLed(int8_t pin, bool active_low, bool on) {
    if (!validOutputPin(pin)) {
        return;
    }
    digitalWrite(pin, active_low ? (on ? LOW : HIGH) : (on ? HIGH : LOW));
}

void FaBoardIoService::recordButtonEvent(uint8_t role, bool is_long) {
    snprintf(state_.last_event,
             sizeof(state_.last_event),
             "%s_%s",
             buttonRoleName(role),
             is_long ? "long" : "press");
    ESP32BASE_LOG_I("farm", "board_button_%s role=%s pin=%d",
                    is_long ? "long" : "press",
                    buttonRoleName(role),
                    role < FA_BOARD_BUTTON_COUNT ? state_.buttons[role].pin : -1);
}
