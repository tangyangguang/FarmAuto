#ifndef FA_BOARD_IO_H
#define FA_BOARD_IO_H

#include <stdint.h>

namespace FaBoardIoConfig {
constexpr const char* NS = "fa_board";
constexpr const char* KEY_ENABLED = "enabled";
constexpr const char* KEY_RUN_LED_PIN = "run_led";
constexpr const char* KEY_ERR_LED_PIN = "err_led";
constexpr const char* KEY_LED_ACTIVE_LOW = "led_low";
constexpr const char* KEY_BOOT_PIN = "boot";
constexpr const char* KEY_BUTTON_1_PIN = "btn1";
constexpr const char* KEY_BUTTON_2_PIN = "btn2";
constexpr const char* KEY_BUTTON_3_PIN = "btn3";
constexpr const char* KEY_BUTTON_4_PIN = "btn4";
constexpr const char* KEY_BUTTON_ACTIVE_LOW = "btn_low";
constexpr const char* KEY_DEBOUNCE_MS = "debounce";
constexpr const char* KEY_LONG_PRESS_MS = "long_ms";
}

enum FaBoardButtonRole : uint8_t {
    FA_BOARD_BUTTON_BOOT = 0u,
    FA_BOARD_BUTTON_RESERVED = 1u,
    FA_BOARD_BUTTON_OPEN = 2u,
    FA_BOARD_BUTTON_CLOSE = 3u,
    FA_BOARD_BUTTON_STOP = 4u,
    FA_BOARD_BUTTON_COUNT = 5u
};

struct FaBoardButtonSnapshot {
    int8_t pin = -1;
    bool configured = false;
    bool pressed = false;
    bool long_pressed = false;
    uint32_t last_change_ms = 0u;
    uint32_t press_started_ms = 0u;
    uint32_t press_count = 0u;
    uint32_t long_press_count = 0u;
};

struct FaBoardIoSnapshot {
    bool enabled = false;
    bool led_active_low = false;
    bool button_active_low = true;
    int8_t run_led_pin = -1;
    int8_t err_led_pin = -1;
    bool run_led_on = false;
    bool err_led_on = false;
    uint16_t debounce_ms = 50u;
    uint16_t long_press_ms = 1000u;
    uint32_t loop_count = 0u;
    char last_event[40] = {};
    FaBoardButtonSnapshot buttons[FA_BOARD_BUTTON_COUNT] = {};
};

class FaBoardIoService {
public:
    struct Config {
        bool enabled = true;
        bool led_active_low = false;
        bool button_active_low = true;
        int8_t run_led_pin = 27;
        int8_t err_led_pin = 14;
        int8_t button_pins[FA_BOARD_BUTTON_COUNT] = {-1, -1, -1, -1, -1};
        uint16_t debounce_ms = 50u;
        uint16_t long_press_ms = 1000u;
    };

    void begin();
    void handle();
    FaBoardIoSnapshot snapshot() const;
    static const char* buttonRoleName(uint8_t role);

private:
    Config readConfig() const;
    void applyConfig(const Config& config);
    void updateLedOutputs(uint32_t now_ms);
    void updateButtons(uint32_t now_ms);
    void setLed(int8_t pin, bool active_low, bool on);
    void recordButtonEvent(uint8_t role, bool is_long);

    FaBoardIoSnapshot state_ = {};
    Config active_config_ = {};
    bool configured_ = false;
    bool raw_button_pressed_[FA_BOARD_BUTTON_COUNT] = {};
    bool stable_button_pressed_[FA_BOARD_BUTTON_COUNT] = {};
    bool long_event_sent_[FA_BOARD_BUTTON_COUNT] = {};
};

#endif
