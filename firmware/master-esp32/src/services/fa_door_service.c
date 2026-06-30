#include "fa_door_service.h"

#include <string.h>

void fa_door_service_init(FaDoorService *service, uint32_t first_action_id) {
    if (service == NULL) {
        return;
    }
    service->next_action_id = first_action_id == 0u ? 1u : first_action_id;
}

uint8_t fa_door_make_motor_config(const FaDoorDeviceConfig *config, FaMasterMotorConfig *motor_config) {
    if (config == NULL || motor_config == NULL || !fa_address_is_normal(config->station_address) ||
        config->travel_pulses == 0u || config->pulses_per_turn == 0u) {
        return FA_STATUS_ERR_BAD_PARAM;
    }

    memset(motor_config, 0, sizeof(*motor_config));
    motor_config->config_version = config->config_version;
    motor_config->pulses_per_turn = config->pulses_per_turn;
    motor_config->default_speed_permille = config->speed_permille;
    motor_config->accel_ms = config->accel_ms;
    motor_config->decel_ms = config->decel_ms;
    motor_config->over_current_ma = config->over_current_ma;
    motor_config->over_current_hold_ms = config->over_current_hold_ms;
    motor_config->stall_detect_ms = config->stall_detect_ms;
    motor_config->stall_min_delta_pulses = config->stall_min_delta_pulses;
    motor_config->max_run_ms = config->max_run_ms;
    motor_config->max_action_pulses = config->max_action_pulses;
    return FA_STATUS_OK;
}

uint8_t fa_door_make_action(FaDoorService *service, const FaDoorDeviceConfig *config, uint8_t command, FaMasterActionRequest *action, FaDoorResult *result) {
    if (service == NULL || config == NULL || action == NULL || result == NULL ||
        !fa_address_is_normal(config->station_address) || config->travel_pulses == 0u ||
        config->travel_pulses > 0x7FFFFFFFul || config->speed_permille > 1000u) {
        return FA_STATUS_ERR_BAD_PARAM;
    }
    if (config->max_action_pulses != 0u && config->travel_pulses > config->max_action_pulses) {
        return FA_STATUS_ERR_SAFETY_BLOCKED;
    }

    int8_t direction = 0;
    uint8_t action_type = 0u;
    if (command == FA_DOOR_COMMAND_OPEN) {
        direction = config->open_direction;
        action_type = FA_ACTION_TYPE_DOOR_OPEN;
    } else if (command == FA_DOOR_COMMAND_CLOSE) {
        direction = config->close_direction;
        action_type = FA_ACTION_TYPE_DOOR_CLOSE;
    } else {
        return FA_STATUS_ERR_BAD_PARAM;
    }
    if (direction == 0) {
        return FA_STATUS_ERR_BAD_PARAM;
    }

    memset(action, 0, sizeof(*action));
    action->action_id = service->next_action_id++;
    if (service->next_action_id == 0u) {
        service->next_action_id = 1u;
    }
    action->device_type = FA_DEVICE_TYPE_DOOR;
    action->action_type = action_type;
    action->target_mode = FA_TARGET_MODE_RELATIVE_PULSES;
    action->start_position_pulses = 0;
    action->target_pulses = (int32_t)config->travel_pulses;
    action->direction = direction;
    action->speed_permille = config->speed_permille;
    action->max_run_ms = config->max_run_ms;
    action->max_action_pulses = config->max_action_pulses;
    action->config_version = config->config_version;

    memset(result, 0, sizeof(*result));
    result->action_id = action->action_id;
    result->target_pulses = config->travel_pulses;
    result->command = command;
    return FA_STATUS_OK;
}
