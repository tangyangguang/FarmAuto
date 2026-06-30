#ifndef FA_DOOR_SERVICE_H
#define FA_DOOR_SERVICE_H

#include "fa_rs485_master.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    FA_DOOR_COMMAND_OPEN = 1u,
    FA_DOOR_COMMAND_CLOSE = 2u
} FaDoorCommand;

typedef struct {
    uint8_t station_address;
    uint16_t config_version;
    uint32_t pulses_per_turn;
    uint32_t travel_pulses;
    int8_t open_direction;
    int8_t close_direction;
    uint16_t speed_permille;
    uint16_t accel_ms;
    uint16_t decel_ms;
    uint16_t over_current_ma;
    uint16_t over_current_hold_ms;
    uint16_t stall_detect_ms;
    uint16_t stall_min_delta_pulses;
    uint32_t max_run_ms;
    uint32_t max_action_pulses;
} FaDoorDeviceConfig;

typedef struct {
    uint32_t next_action_id;
} FaDoorService;

typedef struct {
    uint32_t action_id;
    uint32_t target_pulses;
    uint8_t command;
} FaDoorResult;

void fa_door_service_init(FaDoorService *service, uint32_t first_action_id);
uint8_t fa_door_make_motor_config(const FaDoorDeviceConfig *config, FaMasterMotorConfig *motor_config);
uint8_t fa_door_make_action(FaDoorService *service, const FaDoorDeviceConfig *config, uint8_t command, FaMasterActionRequest *action, FaDoorResult *result);

#ifdef __cplusplus
}
#endif

#endif
