#include "fa_action_controller.h"

#include <stdio.h>

#define CHECK(expr)                                                       \
    do {                                                                  \
        if (!(expr)) {                                                    \
            printf("CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #expr); \
            return 1;                                                     \
        }                                                                 \
    } while (0)

static FaActionConfig default_config(void) {
    FaActionConfig config;
    config.config_version = 3u;
    config.flags = 0u;
    config.max_run_ms = 60000u;
    config.max_action_pulses = 432000u;
    config.over_current_ma = 2000u;
    config.over_current_hold_ms = 100u;
    config.stall_detect_ms = 500u;
    config.stall_min_delta_pulses = 10u;
    return config;
}

static FaActionRequest feed_request(uint32_t action_id, int32_t target_pulses) {
    FaActionRequest request;
    request.action_id = action_id;
    request.device_type = FA_DEVICE_TYPE_FEEDER;
    request.action_type = FA_ACTION_TYPE_FEED;
    request.target_mode = FA_TARGET_MODE_RELATIVE_PULSES;
    request.start_position_pulses = 0;
    request.target_pulses = target_pulses;
    request.direction = 1;
    request.speed_permille = 800u;
    request.max_run_ms = 0u;
    request.max_action_pulses = 0u;
    request.config_version = 3u;
    return request;
}

static int test_relative_action_completes(void) {
    FaActionController controller;
    fa_action_init(&controller);
    FaActionConfig config = default_config();
    CHECK(fa_action_configure(&controller, &config) == FA_STATUS_OK);

    FaActionRequest request = feed_request(1001u, 1000);
    CHECK(fa_action_start(&controller, &request, 0u) == FA_STATUS_OK);

    FaActionOutput output;
    FaActionInputs inputs = {100u, 500, 300u};
    fa_action_tick(&controller, &inputs, &output);
    CHECK(output.motor_enable == 1u);
    CHECK(output.direction == 1);
    CHECK(output.speed_permille == 800u);

    inputs.now_ms = 200u;
    inputs.position_pulses = 1000;
    fa_action_tick(&controller, &inputs, &output);
    CHECK(output.motor_enable == 0u);
    CHECK(output.brake == 1u);

    FaActionStatus status;
    fa_action_get_status(&controller, &inputs, &status);
    CHECK(status.motor_state == FA_MOTOR_COMPLETED);
    CHECK(status.last_stop_reason == FA_STOP_TARGET_REACHED);
    CHECK(status.completed_pulses == 1000u);
    CHECK(status.run_ms == 200u);
    return 0;
}

static int test_stop_command_stops(void) {
    FaActionController controller;
    fa_action_init(&controller);
    FaActionConfig config = default_config();
    CHECK(fa_action_configure(&controller, &config) == FA_STATUS_OK);

    FaActionRequest request = feed_request(1002u, 2000);
    CHECK(fa_action_start(&controller, &request, 0u) == FA_STATUS_OK);
    fa_action_request_stop(&controller, 50u);

    FaActionInputs inputs = {50u, 100, 300u};
    FaActionStatus status;
    fa_action_get_status(&controller, &inputs, &status);
    CHECK(status.motor_state == FA_MOTOR_STOPPED);
    CHECK(status.last_stop_reason == FA_STOP_MASTER_COMMAND);
    CHECK(status.fault_code == FA_FAULT_NONE);
    CHECK(status.run_ms == 50u);
    return 0;
}

static int test_timeout_faults(void) {
    FaActionController controller;
    fa_action_init(&controller);
    FaActionConfig config = default_config();
    config.max_run_ms = 100u;
    CHECK(fa_action_configure(&controller, &config) == FA_STATUS_OK);

    FaActionRequest request = feed_request(1003u, 2000);
    CHECK(fa_action_start(&controller, &request, 0u) == FA_STATUS_OK);

    FaActionInputs inputs = {100u, 100, 300u};
    FaActionOutput output;
    fa_action_tick(&controller, &inputs, &output);

    FaActionStatus status;
    fa_action_get_status(&controller, &inputs, &status);
    CHECK(status.motor_state == FA_MOTOR_FAULT);
    CHECK(status.last_stop_reason == FA_STOP_TIMEOUT);
    CHECK(status.fault_code == FA_FAULT_RUN_TIMEOUT);
    CHECK(status.run_ms == 100u);
    return 0;
}

static int test_stall_faults(void) {
    FaActionController controller;
    fa_action_init(&controller);
    FaActionConfig config = default_config();
    CHECK(fa_action_configure(&controller, &config) == FA_STATUS_OK);

    FaActionRequest request = feed_request(1004u, 2000);
    CHECK(fa_action_start(&controller, &request, 0u) == FA_STATUS_OK);

    FaActionInputs inputs = {500u, 5, 300u};
    FaActionOutput output;
    fa_action_tick(&controller, &inputs, &output);

    FaActionStatus status;
    fa_action_get_status(&controller, &inputs, &status);
    CHECK(status.motor_state == FA_MOTOR_FAULT);
    CHECK(status.last_stop_reason == FA_STOP_STALL);
    CHECK(status.fault_code == FA_FAULT_STALL);
    CHECK(status.run_ms == 500u);
    return 0;
}

static int test_over_current_faults_after_hold(void) {
    FaActionController controller;
    fa_action_init(&controller);
    FaActionConfig config = default_config();
    CHECK(fa_action_configure(&controller, &config) == FA_STATUS_OK);

    FaActionRequest request = feed_request(1005u, 2000);
    CHECK(fa_action_start(&controller, &request, 0u) == FA_STATUS_OK);

    FaActionOutput output;
    FaActionInputs inputs = {50u, 50, 2500u};
    fa_action_tick(&controller, &inputs, &output);
    CHECK(output.motor_enable == 1u);

    inputs.now_ms = 151u;
    inputs.position_pulses = 100;
    fa_action_tick(&controller, &inputs, &output);

    FaActionStatus status;
    fa_action_get_status(&controller, &inputs, &status);
    CHECK(status.motor_state == FA_MOTOR_FAULT);
    CHECK(status.last_stop_reason == FA_STOP_OVER_CURRENT);
    CHECK(status.fault_code == FA_FAULT_OVER_CURRENT);
    CHECK(status.peak_current_ma == 2500u);
    CHECK(status.run_ms == 151u);
    return 0;
}

int main(void) {
    CHECK(test_relative_action_completes() == 0);
    CHECK(test_stop_command_stops() == 0);
    CHECK(test_timeout_faults() == 0);
    CHECK(test_stall_faults() == 0);
    CHECK(test_over_current_faults_after_hold() == 0);
    printf("action smoke tests passed\n");
    return 0;
}
