#ifndef FA_ACTION_CONTROLLER_H
#define FA_ACTION_CONTROLLER_H

#include "fa_protocol.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint16_t config_version;
    uint16_t flags;
    uint32_t max_run_ms;
    uint32_t max_action_pulses;
    uint16_t over_current_ma;
    uint16_t over_current_hold_ms;
    uint16_t stall_detect_ms;
    uint16_t stall_min_delta_pulses;
} FaActionConfig;

typedef struct {
    uint32_t action_id;
    uint8_t device_type;
    uint8_t action_type;
    uint8_t target_mode;
    int32_t start_position_pulses;
    int32_t target_pulses;
    int8_t direction;
    uint16_t speed_permille;
    uint32_t max_run_ms;
    uint32_t max_action_pulses;
    uint16_t config_version;
} FaActionRequest;

typedef struct {
    uint32_t now_ms;
    int32_t position_pulses;
    uint16_t current_ma;
} FaActionInputs;

typedef struct {
    uint8_t motor_enable;
    int8_t direction;
    uint16_t speed_permille;
    uint8_t brake;
} FaActionOutput;

typedef struct {
    uint8_t motor_state;
    uint32_t active_action_id;
    int32_t position_pulses;
    int32_t target_pulses;
    uint16_t current_ma;
    uint16_t peak_current_ma;
    uint32_t run_ms;
    uint32_t completed_pulses;
    uint8_t last_stop_reason;
    uint16_t fault_code;
} FaActionStatus;

typedef struct {
    uint8_t configured;
    FaActionConfig config;
    FaActionRequest request;
    uint8_t motor_state;
    uint16_t fault_code;
    uint8_t last_stop_reason;
    uint32_t start_ms;
    uint32_t end_ms;
    int32_t start_position_pulses;
    int32_t last_motion_position_pulses;
    uint32_t last_motion_check_ms;
    uint8_t over_current_active;
    uint32_t over_current_start_ms;
    uint16_t peak_current_ma;
} FaActionController;

void fa_action_init(FaActionController *controller);
uint8_t fa_action_configure(FaActionController *controller, const FaActionConfig *config);
uint8_t fa_action_start(FaActionController *controller, const FaActionRequest *request, uint32_t now_ms);
void fa_action_request_stop(FaActionController *controller, uint32_t now_ms);
void fa_action_clear_fault(FaActionController *controller);
void fa_action_tick(FaActionController *controller, const FaActionInputs *inputs, FaActionOutput *output);
void fa_action_get_status(const FaActionController *controller, const FaActionInputs *inputs, FaActionStatus *status);

#ifdef __cplusplus
}
#endif

#endif
