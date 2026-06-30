#include "fa_protocol.h"
#include "fa_station_node.h"
#include "stc8h_compiler.h"
#include "stc8h_uart.h"

static STC8H_XDATA FaStationNode g_node;
static STC8H_XDATA stc8h_u8 g_response[FA_MAX_FRAME_LEN];

void main(void) {
    stc8h_u16 i;
    size_t response_len;

    (void)stc8h_uart_init(STC8H_UART1);

    fa_station_node_init(&g_node, FA_ADDRESS_MIN);

    while (1) {
        fa_station_node_tick(&g_node, 0u, 0, 0u);

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
