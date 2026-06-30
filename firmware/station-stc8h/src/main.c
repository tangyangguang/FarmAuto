#include "fa_action_controller.h"
#include "fa_protocol.h"
#include "stc8h_compiler.h"
#include "stc8h_delay.h"
#include "stc8h_uart.h"

static STC8H_XDATA FaActionController g_action;
static STC8H_XDATA FaFrameParser g_parser;

void main(void) {
    (void)stc8h_uart_init(STC8H_UART1);

    fa_action_init(&g_action);
    fa_frame_parser_init(&g_parser);

    stc8h_uart_write_code(STC8H_UART1, "FarmAuto station boot\r\n");

    while (1) {
        stc8h_uart_write_code(STC8H_UART1, "FarmAuto station idle\r\n");
        stc8h_delay_ms(1000u);
    }
}
