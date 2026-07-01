#ifndef FA_STATION_BOARD_H
#define FA_STATION_BOARD_H

#include "fa_action_controller.h"

#include <stdint.h>

#define FA_STATION_BOARD_BACKEND_SIM 1
#define FA_STATION_BOARD_BACKEND_REAL 2

#ifndef FA_STATION_BOARD_BACKEND
#define FA_STATION_BOARD_BACKEND FA_STATION_BOARD_BACKEND_SIM
#endif

#ifdef __cplusplus
extern "C" {
#endif

void fa_station_board_init(uint32_t now_ms);
void fa_station_board_tick(uint32_t now_ms);
void fa_station_board_apply_output(const FaActionOutput *output);
int32_t fa_station_board_position_pulses(void);
uint16_t fa_station_board_current_ma(void);

#ifdef __cplusplus
}
#endif

#endif
