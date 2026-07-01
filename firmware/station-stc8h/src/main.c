#include "fa_protocol.h"
#include "fa_station_board.h"
#include "fa_station_node.h"
#include "stc8h_compiler.h"
#include "stc8h_interrupt.h"
#include "stc8h_timer.h"
#include "stc8h_uart.h"

static STC8H_XDATA FaStationNode g_node;
static STC8H_XDATA stc8h_u8 g_response[FA_MAX_FRAME_LEN];
static volatile stc8h_u32 g_now_ms;

STC8H_INTERRUPT(timer0_isr, STC8H_VECTOR_TIMER0)
{
    stc8h_timer_clear_flag(STC8H_TIMER0);
    ++g_now_ms;
}

static stc8h_u32 station_now_ms(void) {
    stc8h_u32 now;

    stc8h_interrupt_disable_global();
    now = g_now_ms;
    stc8h_interrupt_enable_global();
    return now;
}

static void update_status_leds(void) {
    uint8_t run_on;
    uint8_t err_on;

    err_on = (g_node.action.motor_state == FA_MOTOR_FAULT ||
              g_node.action.fault_code != FA_FAULT_NONE ||
              !fa_address_is_normal(g_node.raw_address_input)) ? 1u : 0u;
    run_on = err_on == 0u ? 1u : 0u;
    fa_station_board_set_leds(run_on, err_on);
}

void main(void) {
    stc8h_u16 i;
    size_t response_len;
    uint32_t now_ms;

    (void)stc8h_uart_init(STC8H_UART1);

    if (stc8h_timer_init_1ms(STC8H_TIMER0) != STC8H_OK) {
        while (1) {
        }
    }
    stc8h_timer_enable_interrupt(STC8H_TIMER0);
    stc8h_interrupt_enable_global();
    stc8h_timer_start(STC8H_TIMER0);

    fa_station_board_init(station_now_ms());
    fa_station_node_init(&g_node, fa_station_board_address_input());
    update_status_leds();

    while (1) {
        now_ms = station_now_ms();
        fa_station_board_tick(now_ms);
        fa_station_node_tick(&g_node,
                             now_ms,
                             fa_station_board_position_pulses(),
                             fa_station_board_current_ma());
        fa_station_board_apply_output(&g_node.output);
        update_status_leds();

        if (stc8h_uart_readable(STC8H_UART1) != 0u) {
            response_len = 0u;
            if (fa_station_node_push_byte(&g_node,
                                          (stc8h_u8)stc8h_uart_getc(STC8H_UART1),
                                          g_response,
                                          sizeof(g_response),
                                          &response_len) == FA_FRAME_OK) {
                for (i = 0u; i < response_len; ++i) {
                    stc8h_uart_putc(STC8H_UART1, (char)g_response[i]);
                }
            }
        }
    }
}
