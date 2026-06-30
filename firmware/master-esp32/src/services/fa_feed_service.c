#include "fa_feed_service.h"

#include <string.h>

void fa_feed_service_init(FaFeedService *service, uint32_t first_action_id) {
    if (service == NULL) {
        return;
    }
    service->next_action_id = first_action_id == 0u ? 1u : first_action_id;
}

uint8_t fa_feed_calculate_target_pulses(const FaFeedDeviceConfig *config, uint8_t mode, uint32_t amount, uint32_t *target_pulses) {
    if (config == NULL || target_pulses == NULL || amount == 0u || config->pulses_per_turn == 0u) {
        return FA_STATUS_ERR_BAD_PARAM;
    }

    uint64_t pulses = 0u;
    if (mode == FA_FEED_AMOUNT_TURNS_X1000) {
        pulses = ((uint64_t)amount * (uint64_t)config->pulses_per_turn) / 1000u;
    } else if (mode == FA_FEED_AMOUNT_MG) {
        if (config->grams_per_turn_mg == 0u) {
            return FA_STATUS_ERR_BAD_PARAM;
        }
        pulses = ((uint64_t)amount * (uint64_t)config->pulses_per_turn) / (uint64_t)config->grams_per_turn_mg;
    } else {
        return FA_STATUS_ERR_BAD_PARAM;
    }

    if (pulses == 0u || pulses > 0x7FFFFFFFul) {
        return FA_STATUS_ERR_BAD_PARAM;
    }
    if (config->max_action_pulses != 0u && pulses > config->max_action_pulses) {
        return FA_STATUS_ERR_SAFETY_BLOCKED;
    }

    *target_pulses = (uint32_t)pulses;
    return FA_STATUS_OK;
}

uint8_t fa_feed_make_motor_config(const FaFeedDeviceConfig *config, FaMasterMotorConfig *motor_config) {
    if (config == NULL || motor_config == NULL || config->station_address == FA_MASTER_ADDRESS) {
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

uint8_t fa_feed_make_manual_action(FaFeedService *service, const FaFeedDeviceConfig *config, uint8_t mode, uint32_t amount, FaMasterActionRequest *action, FaFeedResult *result) {
    if (service == NULL || config == NULL || action == NULL || result == NULL) {
        return FA_STATUS_ERR_BAD_PARAM;
    }
    if (config->feed_direction == 0 || config->speed_permille > 1000u) {
        return FA_STATUS_ERR_BAD_PARAM;
    }

    uint32_t target_pulses = 0u;
    uint8_t status = fa_feed_calculate_target_pulses(config, mode, amount, &target_pulses);
    if (status != FA_STATUS_OK) {
        return status;
    }

    memset(action, 0, sizeof(*action));
    action->action_id = service->next_action_id++;
    if (service->next_action_id == 0u) {
        service->next_action_id = 1u;
    }
    action->device_type = FA_DEVICE_TYPE_FEEDER;
    action->action_type = FA_ACTION_TYPE_FEED;
    action->target_mode = FA_TARGET_MODE_RELATIVE_PULSES;
    action->start_position_pulses = 0;
    action->target_pulses = (int32_t)target_pulses;
    action->direction = config->feed_direction;
    action->speed_permille = config->speed_permille;
    action->max_run_ms = config->max_run_ms;
    action->max_action_pulses = config->max_action_pulses;
    action->config_version = config->config_version;

    memset(result, 0, sizeof(*result));
    result->action_id = action->action_id;
    result->target_pulses = target_pulses;
    return FA_STATUS_OK;
}

uint8_t fa_feed_result_from_status(const FaMasterStatusResponse *status, FaFeedResult *result) {
    if (status == NULL || result == NULL) {
        return FA_STATUS_ERR_BAD_PARAM;
    }
    if (status->active_action_id != result->action_id) {
        return FA_STATUS_ERR_BAD_PARAM;
    }

    result->completed_pulses = status->completed_pulses;
    result->stop_reason = status->last_stop_reason;
    result->fault_code = status->common.fault_code;
    result->completed = status->motor_state == FA_MOTOR_COMPLETED ? 1u : 0u;
    result->failed = status->motor_state == FA_MOTOR_FAULT || status->common.fault_code != FA_FAULT_NONE ? 1u : 0u;
    return FA_STATUS_OK;
}
