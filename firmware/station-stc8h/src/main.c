#include "fa_protocol.h"
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

void main(void) {
    stc8h_u16 i;
    size_t response_len;

    (void)stc8h_uart_init(STC8H_UART1);

    if (stc8h_timer_init_1ms(STC8H_TIMER0) != STC8H_OK) {
        while (1) {
        }
    }
    stc8h_timer_enable_interrupt(STC8H_TIMER0);
    stc8h_interrupt_enable_global();
    stc8h_timer_start(STC8H_TIMER0);

    fa_station_node_init(&g_node, FA_ADDRESS_MIN);

    while (1) {
        fa_station_node_tick(&g_node, station_now_ms(), 0, 0u);

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
