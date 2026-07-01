#include "fa_station_board.h"

#include <string.h>

#ifndef FA_STATION_SIM_MAX_PULSES_PER_SEC
#define FA_STATION_SIM_MAX_PULSES_PER_SEC 6000u
#endif

#ifndef FA_STATION_SIM_IDLE_CURRENT_MA
#define FA_STATION_SIM_IDLE_CURRENT_MA 20u
#endif

#ifndef FA_STATION_SIM_BASE_RUN_CURRENT_MA
#define FA_STATION_SIM_BASE_RUN_CURRENT_MA 120u
#endif

typedef struct {
    int32_t position_pulses;
    uint16_t current_ma;
    uint32_t last_update_ms;
    uint32_t pulse_remainder_x1000;
    FaActionOutput output;
} FaStationBoardSim;

static FaStationBoardSim g_board;

void fa_station_board_init(uint32_t now_ms) {
    memset(&g_board, 0, sizeof(g_board));
    g_board.current_ma = FA_STATION_SIM_IDLE_CURRENT_MA;
    g_board.last_update_ms = now_ms;
}

void fa_station_board_tick(uint32_t now_ms) {
    uint32_t elapsed_ms;
    uint32_t step_x1000;
    uint32_t step_pulses;

    elapsed_ms = now_ms - g_board.last_update_ms;
    g_board.last_update_ms = now_ms;

    if (g_board.output.motor_enable == 0u || g_board.output.speed_permille == 0u || elapsed_ms == 0u) {
        g_board.current_ma = FA_STATION_SIM_IDLE_CURRENT_MA;
        return;
    }

    step_x1000 = elapsed_ms *
                 (uint32_t)g_board.output.speed_permille *
                 FA_STATION_SIM_MAX_PULSES_PER_SEC;
    step_x1000 = step_x1000 / 1000u + g_board.pulse_remainder_x1000;
    step_pulses = step_x1000 / 1000u;
    g_board.pulse_remainder_x1000 = step_x1000 % 1000u;

    if (step_pulses != 0u) {
        if (g_board.output.direction > 0) {
            g_board.position_pulses += (int32_t)step_pulses;
        } else if (g_board.output.direction < 0) {
            g_board.position_pulses -= (int32_t)step_pulses;
        }
    }

    g_board.current_ma = (uint16_t)(FA_STATION_SIM_BASE_RUN_CURRENT_MA +
                                    (g_board.output.speed_permille / 2u));
}

void fa_station_board_apply_output(const FaActionOutput *output) {
    if (output == 0) {
        memset(&g_board.output, 0, sizeof(g_board.output));
        return;
    }
    g_board.output = *output;
    if (g_board.output.motor_enable == 0u) {
        g_board.pulse_remainder_x1000 = 0u;
    }
}

int32_t fa_station_board_position_pulses(void) {
    return g_board.position_pulses;
}

uint16_t fa_station_board_current_ma(void) {
    return g_board.current_ma;
}
