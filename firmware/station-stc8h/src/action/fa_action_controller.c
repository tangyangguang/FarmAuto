#include "fa_action_controller.h"

#include <string.h>

static uint32_t fa_abs_i32_delta(int32_t a, int32_t b) {
    const int32_t delta = a - b;
    if (delta >= 0) {
        return (uint32_t)delta;
    }
    return (uint32_t)(-delta);
}

static uint32_t fa_action_effective_max_run_ms(const FaActionController *controller) {
    return controller->request.max_run_ms != 0u ? controller->request.max_run_ms : controller->config.max_run_ms;
}

static uint32_t fa_action_effective_max_pulses(const FaActionController *controller) {
    return controller->request.max_action_pulses != 0u ? controller->request.max_action_pulses : controller->config.max_action_pulses;
}

static uint32_t fa_action_completed_pulses(const FaActionController *controller, int32_t position_pulses) {
    return fa_abs_i32_delta(position_pulses, controller->start_position_pulses);
}

static int fa_action_target_reached(const FaActionController *controller, int32_t position_pulses) {
    if (controller->request.target_mode == FA_TARGET_MODE_ABSOLUTE_POSITION) {
        if (controller->request.direction > 0) {
            return position_pulses >= controller->request.target_pulses;
        }
        return position_pulses <= controller->request.target_pulses;
    }

    return fa_action_completed_pulses(controller, position_pulses) >= fa_abs_i32_delta(controller->request.target_pulses, 0);
}

static int fa_action_is_terminal(uint8_t motor_state) {
    return motor_state == FA_MOTOR_COMPLETED ||
           motor_state == FA_MOTOR_STOPPED ||
           motor_state == FA_MOTOR_FAULT;
}

static void fa_action_finish(FaActionController *controller, uint32_t now_ms, uint8_t motor_state, uint8_t stop_reason, uint16_t fault_code) {
    controller->motor_state = motor_state;
    controller->last_stop_reason = stop_reason;
    controller->fault_code = fault_code;
    controller->end_ms = now_ms;
    controller->over_current_active = 0u;
}

static void fa_action_fill_output(const FaActionController *controller, FaActionOutput *output) {
    if (output == NULL) {
        return;
    }

    output->motor_enable = controller->motor_state == FA_MOTOR_RUNNING ? 1u : 0u;
    output->direction = output->motor_enable != 0u ? controller->request.direction : 0;
    output->speed_permille = output->motor_enable != 0u ? controller->request.speed_permille : 0u;
    output->brake = controller->motor_state == FA_MOTOR_COMPLETED ||
                    controller->motor_state == FA_MOTOR_STOPPED ||
                    controller->motor_state == FA_MOTOR_FAULT;
}

void fa_action_init(FaActionController *controller) {
    if (controller == NULL) {
        return;
    }
    memset(controller, 0, sizeof(*controller));
    controller->motor_state = FA_MOTOR_UNCONFIGURED;
    controller->last_stop_reason = FA_STOP_NONE;
    controller->fault_code = FA_FAULT_NONE;
}

uint8_t fa_action_configure(FaActionController *controller, const FaActionConfig *config) {
    if (controller == NULL || config == NULL) {
        return FA_STATUS_ERR_BAD_PARAM;
    }
    if (config->max_action_pulses == 0u || config->max_run_ms == 0u) {
        return FA_STATUS_ERR_BAD_PARAM;
    }

    controller->config = *config;
    controller->configured = 1u;
    if (controller->motor_state == FA_MOTOR_UNCONFIGURED) {
        controller->motor_state = FA_MOTOR_IDLE;
    }
    return FA_STATUS_OK;
}

uint8_t fa_action_start(FaActionController *controller, const FaActionRequest *request, uint32_t now_ms) {
    if (controller == NULL || request == NULL) {
        return FA_STATUS_ERR_BAD_PARAM;
    }
    if (controller->configured == 0u) {
        return FA_STATUS_ERR_NOT_CONFIGURED;
    }
    if (controller->motor_state == FA_MOTOR_RUNNING || controller->motor_state == FA_MOTOR_STOPPING) {
        if (controller->request.action_id == request->action_id) {
            return FA_STATUS_ERR_ACTION_DUPLICATE;
        }
        return FA_STATUS_ERR_BUSY;
    }
    if (controller->motor_state == FA_MOTOR_FAULT) {
        return FA_STATUS_ERR_FAULT_ACTIVE;
    }
    if (request->config_version != controller->config.config_version ||
        request->direction == 0 ||
        request->speed_permille > 1000u ||
        request->target_mode < FA_TARGET_MODE_RELATIVE_PULSES ||
        request->target_mode > FA_TARGET_MODE_ABSOLUTE_POSITION) {
        return FA_STATUS_ERR_BAD_PARAM;
    }

    const uint32_t requested_pulses = request->target_mode == FA_TARGET_MODE_RELATIVE_PULSES
                                         ? fa_abs_i32_delta(request->target_pulses, 0)
                                         : fa_abs_i32_delta(request->target_pulses, request->start_position_pulses);
    const uint32_t max_pulses = request->max_action_pulses != 0u ? request->max_action_pulses : controller->config.max_action_pulses;
    if (requested_pulses == 0u || requested_pulses > max_pulses) {
        return FA_STATUS_ERR_BAD_PARAM;
    }

    controller->request = *request;
    controller->motor_state = FA_MOTOR_RUNNING;
    controller->fault_code = FA_FAULT_NONE;
    controller->last_stop_reason = FA_STOP_NONE;
    controller->start_ms = now_ms;
    controller->end_ms = 0u;
    controller->start_position_pulses = request->start_position_pulses;
    controller->last_motion_position_pulses = request->start_position_pulses;
    controller->last_motion_check_ms = now_ms;
    controller->over_current_active = 0u;
    controller->over_current_start_ms = 0u;
    controller->peak_current_ma = 0u;
    return FA_STATUS_OK;
}

void fa_action_request_stop(FaActionController *controller, uint32_t now_ms) {
    (void)now_ms;
    if (controller == NULL) {
        return;
    }
    if (controller->motor_state == FA_MOTOR_RUNNING || controller->motor_state == FA_MOTOR_STOPPING) {
        fa_action_finish(controller, now_ms, FA_MOTOR_STOPPED, FA_STOP_MASTER_COMMAND, FA_FAULT_NONE);
    }
}

void fa_action_clear_fault(FaActionController *controller) {
    if (controller == NULL) {
        return;
    }
    if (controller->motor_state == FA_MOTOR_FAULT) {
        controller->motor_state = controller->configured != 0u ? FA_MOTOR_IDLE : FA_MOTOR_UNCONFIGURED;
    }
    controller->fault_code = FA_FAULT_NONE;
    controller->last_stop_reason = FA_STOP_NONE;
    controller->over_current_active = 0u;
}

void fa_action_tick(FaActionController *controller, const FaActionInputs *inputs, FaActionOutput *output) {
    if (controller == NULL || inputs == NULL) {
        return;
    }

    if (inputs->current_ma > controller->peak_current_ma) {
        controller->peak_current_ma = inputs->current_ma;
    }

    if (controller->motor_state != FA_MOTOR_RUNNING) {
        fa_action_fill_output(controller, output);
        return;
    }

    if (fa_action_target_reached(controller, inputs->position_pulses)) {
        fa_action_finish(controller, inputs->now_ms, FA_MOTOR_COMPLETED, FA_STOP_TARGET_REACHED, FA_FAULT_NONE);
        fa_action_fill_output(controller, output);
        return;
    }

    const uint32_t completed_pulses = fa_action_completed_pulses(controller, inputs->position_pulses);
    const uint32_t max_pulses = fa_action_effective_max_pulses(controller);
    if (max_pulses != 0u && completed_pulses > max_pulses) {
        fa_action_finish(controller, inputs->now_ms, FA_MOTOR_FAULT, FA_STOP_TARGET_OVERRUN, FA_FAULT_TARGET_OVERRUN);
        fa_action_fill_output(controller, output);
        return;
    }

    const uint32_t run_ms = inputs->now_ms - controller->start_ms;
    const uint32_t max_run_ms = fa_action_effective_max_run_ms(controller);
    if (max_run_ms != 0u && run_ms >= max_run_ms) {
        fa_action_finish(controller, inputs->now_ms, FA_MOTOR_FAULT, FA_STOP_TIMEOUT, FA_FAULT_RUN_TIMEOUT);
        fa_action_fill_output(controller, output);
        return;
    }

    if (controller->config.over_current_ma != 0u && inputs->current_ma >= controller->config.over_current_ma) {
        if (controller->over_current_active == 0u) {
            controller->over_current_active = 1u;
            controller->over_current_start_ms = inputs->now_ms;
        } else if (inputs->now_ms - controller->over_current_start_ms >= controller->config.over_current_hold_ms) {
            fa_action_finish(controller, inputs->now_ms, FA_MOTOR_FAULT, FA_STOP_OVER_CURRENT, FA_FAULT_OVER_CURRENT);
            fa_action_fill_output(controller, output);
            return;
        }
    } else {
        controller->over_current_active = 0u;
    }

    if (controller->config.stall_detect_ms != 0u &&
        inputs->now_ms - controller->last_motion_check_ms >= controller->config.stall_detect_ms) {
        const uint32_t delta = fa_abs_i32_delta(inputs->position_pulses, controller->last_motion_position_pulses);
        if (delta < controller->config.stall_min_delta_pulses) {
            fa_action_finish(controller, inputs->now_ms, FA_MOTOR_FAULT, FA_STOP_STALL, FA_FAULT_STALL);
            fa_action_fill_output(controller, output);
            return;
        }
        controller->last_motion_position_pulses = inputs->position_pulses;
        controller->last_motion_check_ms = inputs->now_ms;
    }

    fa_action_fill_output(controller, output);
}

void fa_action_get_status(const FaActionController *controller, const FaActionInputs *inputs, FaActionStatus *status) {
    if (controller == NULL || status == NULL) {
        return;
    }

    memset(status, 0, sizeof(*status));
    status->motor_state = controller->motor_state;
    status->active_action_id = controller->request.action_id;
    status->position_pulses = inputs != NULL ? inputs->position_pulses : controller->start_position_pulses;
    status->target_pulses = controller->request.target_pulses;
    status->current_ma = inputs != NULL ? inputs->current_ma : 0u;
    status->peak_current_ma = controller->peak_current_ma;
    if (inputs != NULL && controller->motor_state == FA_MOTOR_RUNNING) {
        status->run_ms = inputs->now_ms - controller->start_ms;
    } else if (fa_action_is_terminal(controller->motor_state) && controller->end_ms >= controller->start_ms) {
        status->run_ms = controller->end_ms - controller->start_ms;
    } else {
        status->run_ms = 0u;
    }
    status->completed_pulses = fa_action_completed_pulses(controller, status->position_pulses);
    status->last_stop_reason = controller->last_stop_reason;
    status->fault_code = controller->fault_code;
}
