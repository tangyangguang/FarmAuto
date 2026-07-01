#include "fa_station_board.h"

#if FA_STATION_BOARD_BACKEND == FA_STATION_BOARD_BACKEND_REAL

#include "stc8h_adc.h"
#include "stc8h_gpio.h"
#include "stc8h_pwm.h"

#ifndef FA_BOARD_AT8236_IN1_PORT
#error "FA_BOARD_AT8236_IN1_PORT is required for FA_STATION_BOARD_BACKEND_REAL"
#endif
#ifndef FA_BOARD_AT8236_IN1_PIN
#error "FA_BOARD_AT8236_IN1_PIN is required for FA_STATION_BOARD_BACKEND_REAL"
#endif
#ifndef FA_BOARD_AT8236_IN2_PORT
#error "FA_BOARD_AT8236_IN2_PORT is required for FA_STATION_BOARD_BACKEND_REAL"
#endif
#ifndef FA_BOARD_AT8236_IN2_PIN
#error "FA_BOARD_AT8236_IN2_PIN is required for FA_STATION_BOARD_BACKEND_REAL"
#endif
#ifndef FA_BOARD_PWM_GROUP
#error "FA_BOARD_PWM_GROUP is required for FA_STATION_BOARD_BACKEND_REAL"
#endif
#ifndef FA_BOARD_PWM_CHANNEL
#error "FA_BOARD_PWM_CHANNEL is required for FA_STATION_BOARD_BACKEND_REAL"
#endif
#ifndef FA_BOARD_PWM_PIN_SELECT
#error "FA_BOARD_PWM_PIN_SELECT is required for FA_STATION_BOARD_BACKEND_REAL"
#endif
#ifndef FA_BOARD_CURRENT_ADC_CHANNEL
#error "FA_BOARD_CURRENT_ADC_CHANNEL is required for FA_STATION_BOARD_BACKEND_REAL"
#endif
#ifndef FA_BOARD_ENCODER_IMPLEMENTED
#error "FA_BOARD_ENCODER_IMPLEMENTED is required for FA_STATION_BOARD_BACKEND_REAL"
#endif
#ifndef FA_BOARD_RUN_LED_PORT
#error "FA_BOARD_RUN_LED_PORT is required for FA_STATION_BOARD_BACKEND_REAL"
#endif
#ifndef FA_BOARD_RUN_LED_PIN
#error "FA_BOARD_RUN_LED_PIN is required for FA_STATION_BOARD_BACKEND_REAL"
#endif
#ifndef FA_BOARD_ERR_LED_PORT
#error "FA_BOARD_ERR_LED_PORT is required for FA_STATION_BOARD_BACKEND_REAL"
#endif
#ifndef FA_BOARD_ERR_LED_PIN
#error "FA_BOARD_ERR_LED_PIN is required for FA_STATION_BOARD_BACKEND_REAL"
#endif
#ifndef FA_BOARD_ADDR0_PORT
#error "FA_BOARD_ADDR0_PORT is required for FA_STATION_BOARD_BACKEND_REAL"
#endif
#ifndef FA_BOARD_ADDR0_PIN
#error "FA_BOARD_ADDR0_PIN is required for FA_STATION_BOARD_BACKEND_REAL"
#endif
#ifndef FA_BOARD_ADDR1_PORT
#error "FA_BOARD_ADDR1_PORT is required for FA_STATION_BOARD_BACKEND_REAL"
#endif
#ifndef FA_BOARD_ADDR1_PIN
#error "FA_BOARD_ADDR1_PIN is required for FA_STATION_BOARD_BACKEND_REAL"
#endif
#ifndef FA_BOARD_ADDR2_PORT
#error "FA_BOARD_ADDR2_PORT is required for FA_STATION_BOARD_BACKEND_REAL"
#endif
#ifndef FA_BOARD_ADDR2_PIN
#error "FA_BOARD_ADDR2_PIN is required for FA_STATION_BOARD_BACKEND_REAL"
#endif
#ifndef FA_BOARD_ADDR3_PORT
#error "FA_BOARD_ADDR3_PORT is required for FA_STATION_BOARD_BACKEND_REAL"
#endif
#ifndef FA_BOARD_ADDR3_PIN
#error "FA_BOARD_ADDR3_PIN is required for FA_STATION_BOARD_BACKEND_REAL"
#endif
#ifndef FA_BOARD_ADDR4_PORT
#error "FA_BOARD_ADDR4_PORT is required for FA_STATION_BOARD_BACKEND_REAL"
#endif
#ifndef FA_BOARD_ADDR4_PIN
#error "FA_BOARD_ADDR4_PIN is required for FA_STATION_BOARD_BACKEND_REAL"
#endif
#ifndef FA_BOARD_ADDR5_PORT
#error "FA_BOARD_ADDR5_PORT is required for FA_STATION_BOARD_BACKEND_REAL"
#endif
#ifndef FA_BOARD_ADDR5_PIN
#error "FA_BOARD_ADDR5_PIN is required for FA_STATION_BOARD_BACKEND_REAL"
#endif
#ifndef FA_BOARD_ADDR6_PORT
#error "FA_BOARD_ADDR6_PORT is required for FA_STATION_BOARD_BACKEND_REAL"
#endif
#ifndef FA_BOARD_ADDR6_PIN
#error "FA_BOARD_ADDR6_PIN is required for FA_STATION_BOARD_BACKEND_REAL"
#endif

#ifndef FA_BOARD_PWM_PERIOD
#define FA_BOARD_PWM_PERIOD 1000u
#endif

static int32_t g_position_pulses;
static uint16_t g_current_ma;

static uint8_t read_addr_bit(uint8_t port, uint8_t pin, uint8_t bit_value) {
    return stc8h_gpio_read(port, pin) != 0u ? bit_value : 0u;
}

static void motor_coast(void) {
    (void)stc8h_pwm_set_duty(FA_BOARD_PWM_GROUP, FA_BOARD_PWM_CHANNEL, 0u);
    stc8h_gpio_write(FA_BOARD_AT8236_IN1_PORT, FA_BOARD_AT8236_IN1_PIN, 0u);
    stc8h_gpio_write(FA_BOARD_AT8236_IN2_PORT, FA_BOARD_AT8236_IN2_PIN, 0u);
}

static void motor_brake(void) {
    (void)stc8h_pwm_set_duty(FA_BOARD_PWM_GROUP, FA_BOARD_PWM_CHANNEL, 0u);
    stc8h_gpio_write(FA_BOARD_AT8236_IN1_PORT, FA_BOARD_AT8236_IN1_PIN, 1u);
    stc8h_gpio_write(FA_BOARD_AT8236_IN2_PORT, FA_BOARD_AT8236_IN2_PIN, 1u);
}

void fa_station_board_init(uint32_t now_ms) {
    (void)now_ms;
    g_position_pulses = 0;
    g_current_ma = 0u;

    stc8h_gpio_set_mode(FA_BOARD_AT8236_IN1_PORT, FA_BOARD_AT8236_IN1_PIN, STC8H_GPIO_MODE_PUSH_PULL);
    stc8h_gpio_set_mode(FA_BOARD_AT8236_IN2_PORT, FA_BOARD_AT8236_IN2_PIN, STC8H_GPIO_MODE_PUSH_PULL);
    stc8h_gpio_set_mode(FA_BOARD_RUN_LED_PORT, FA_BOARD_RUN_LED_PIN, STC8H_GPIO_MODE_PUSH_PULL);
    stc8h_gpio_set_mode(FA_BOARD_ERR_LED_PORT, FA_BOARD_ERR_LED_PIN, STC8H_GPIO_MODE_PUSH_PULL);
    stc8h_gpio_set_mode(FA_BOARD_ADDR0_PORT, FA_BOARD_ADDR0_PIN, STC8H_GPIO_MODE_INPUT_ONLY);
    stc8h_gpio_set_mode(FA_BOARD_ADDR1_PORT, FA_BOARD_ADDR1_PIN, STC8H_GPIO_MODE_INPUT_ONLY);
    stc8h_gpio_set_mode(FA_BOARD_ADDR2_PORT, FA_BOARD_ADDR2_PIN, STC8H_GPIO_MODE_INPUT_ONLY);
    stc8h_gpio_set_mode(FA_BOARD_ADDR3_PORT, FA_BOARD_ADDR3_PIN, STC8H_GPIO_MODE_INPUT_ONLY);
    stc8h_gpio_set_mode(FA_BOARD_ADDR4_PORT, FA_BOARD_ADDR4_PIN, STC8H_GPIO_MODE_INPUT_ONLY);
    stc8h_gpio_set_mode(FA_BOARD_ADDR5_PORT, FA_BOARD_ADDR5_PIN, STC8H_GPIO_MODE_INPUT_ONLY);
    stc8h_gpio_set_mode(FA_BOARD_ADDR6_PORT, FA_BOARD_ADDR6_PIN, STC8H_GPIO_MODE_INPUT_ONLY);
    fa_station_board_set_leds(0u, 0u);
    motor_coast();

    (void)stc8h_pwm_set_period(FA_BOARD_PWM_GROUP, FA_BOARD_PWM_PERIOD);
    (void)stc8h_pwm_init_channel(FA_BOARD_PWM_GROUP, FA_BOARD_PWM_CHANNEL, FA_BOARD_PWM_PIN_SELECT);
    (void)stc8h_pwm_set_duty(FA_BOARD_PWM_GROUP, FA_BOARD_PWM_CHANNEL, 0u);
    (void)stc8h_pwm_enable(FA_BOARD_PWM_GROUP, FA_BOARD_PWM_CHANNEL);

    stc8h_adc_init();
}

void fa_station_board_tick(uint32_t now_ms) {
    uint16_t adc;
    (void)now_ms;

    adc = stc8h_adc_read(FA_BOARD_CURRENT_ADC_CHANNEL);
    g_current_ma = adc == STC8H_ADC_INVALID_VALUE ? 0u : adc;
}

void fa_station_board_apply_output(const FaActionOutput *output) {
    uint32_t duty;

    if (output == 0 || output->motor_enable == 0u) {
        if (output != 0 && output->brake != 0u) {
            motor_brake();
        } else {
            motor_coast();
        }
        return;
    }

    duty = ((uint32_t)FA_BOARD_PWM_PERIOD * output->speed_permille) / 1000u;
    if (output->direction > 0) {
        stc8h_gpio_write(FA_BOARD_AT8236_IN1_PORT, FA_BOARD_AT8236_IN1_PIN, 1u);
        stc8h_gpio_write(FA_BOARD_AT8236_IN2_PORT, FA_BOARD_AT8236_IN2_PIN, 0u);
    } else {
        stc8h_gpio_write(FA_BOARD_AT8236_IN1_PORT, FA_BOARD_AT8236_IN1_PIN, 0u);
        stc8h_gpio_write(FA_BOARD_AT8236_IN2_PORT, FA_BOARD_AT8236_IN2_PIN, 1u);
    }
    (void)stc8h_pwm_set_duty(FA_BOARD_PWM_GROUP, FA_BOARD_PWM_CHANNEL, (uint16_t)duty);
}

uint8_t fa_station_board_address_input(void) {
    uint8_t address = 0u;
    address |= read_addr_bit(FA_BOARD_ADDR0_PORT, FA_BOARD_ADDR0_PIN, 0x01u);
    address |= read_addr_bit(FA_BOARD_ADDR1_PORT, FA_BOARD_ADDR1_PIN, 0x02u);
    address |= read_addr_bit(FA_BOARD_ADDR2_PORT, FA_BOARD_ADDR2_PIN, 0x04u);
    address |= read_addr_bit(FA_BOARD_ADDR3_PORT, FA_BOARD_ADDR3_PIN, 0x08u);
    address |= read_addr_bit(FA_BOARD_ADDR4_PORT, FA_BOARD_ADDR4_PIN, 0x10u);
    address |= read_addr_bit(FA_BOARD_ADDR5_PORT, FA_BOARD_ADDR5_PIN, 0x20u);
    address |= read_addr_bit(FA_BOARD_ADDR6_PORT, FA_BOARD_ADDR6_PIN, 0x40u);
    return address;
}

void fa_station_board_set_leds(uint8_t run_on, uint8_t err_on) {
    stc8h_gpio_write(FA_BOARD_RUN_LED_PORT, FA_BOARD_RUN_LED_PIN, run_on != 0u ? 1u : 0u);
    stc8h_gpio_write(FA_BOARD_ERR_LED_PORT, FA_BOARD_ERR_LED_PIN, err_on != 0u ? 1u : 0u);
}

int32_t fa_station_board_position_pulses(void) {
    return g_position_pulses;
}

uint16_t fa_station_board_current_ma(void) {
    return g_current_ma;
}

#endif
