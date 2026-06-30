#include "fa_rs485_master.h"
#include "fake_station.h"

#include <stdio.h>

#define CHECK(expr)                                                       \
    do {                                                                  \
        if (!(expr)) {                                                    \
            printf("CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #expr); \
            return 1;                                                     \
        }                                                                 \
    } while (0)

static int transact(FakeStation *station, const uint8_t *request, size_t request_len, uint8_t *response, size_t response_cap, size_t *response_len) {
    CHECK(fake_station_handle(station, request, request_len, response, response_cap, response_len) == FA_FRAME_OK);
    return 0;
}

int main(void) {
    FaRs485Master master;
    fa_rs485_master_init(&master);

    FakeStation station;
    fake_station_init(&station, 7u);

    uint8_t request[FA_MAX_FRAME_LEN];
    uint8_t response[FA_MAX_FRAME_LEN];
    size_t request_len = 0u;
    size_t response_len = 0u;
    uint8_t seq = 0u;

    CHECK(fa_rs485_master_build_ping(&master, 7u, request, sizeof(request), &request_len, &seq) == FA_FRAME_OK);
    CHECK(transact(&station, request, request_len, response, sizeof(response), &response_len) == 0);
    FaMasterPingResponse ping;
    CHECK(fa_rs485_master_parse_ping(response, response_len, 7u, seq, &ping) == FA_STATUS_OK);
    CHECK(ping.device_class == FA_DEVICE_CLASS_MOTOR_ACTUATOR);
    CHECK((ping.capability_flags & FA_CAP_MOTOR_BIDIRECTIONAL) != 0u);
    CHECK(ping.max_payload_len == FA_MAX_PAYLOAD_LEN);

    FaMasterMotorConfig config;
    config.config_version = 3u;
    config.flags = 0u;
    config.pulses_per_turn = 4320u;
    config.default_speed_permille = 800u;
    config.accel_ms = 1000u;
    config.decel_ms = 500u;
    config.over_current_ma = 2000u;
    config.over_current_hold_ms = 100u;
    config.stall_detect_ms = 500u;
    config.stall_min_delta_pulses = 10u;
    config.max_run_ms = 60000u;
    config.max_action_pulses = 432000u;

    CHECK(fa_rs485_master_build_set_motor_config(&master, 7u, &config, request, sizeof(request), &request_len, &seq) == FA_FRAME_OK);
    CHECK(transact(&station, request, request_len, response, sizeof(response), &response_len) == 0);
    FaMasterCommonResponse common;
    CHECK(fa_rs485_master_parse_common(response, response_len, 7u, seq, FA_CMD_SET_MOTOR_CONFIG, &common) == FA_STATUS_OK);
    CHECK(common.status_code == FA_STATUS_OK);

    FaMasterActionRequest action;
    action.action_id = 9001u;
    action.device_type = FA_DEVICE_TYPE_FEEDER;
    action.action_type = FA_ACTION_TYPE_FEED;
    action.target_mode = FA_TARGET_MODE_RELATIVE_PULSES;
    action.start_position_pulses = 0;
    action.target_pulses = 1200;
    action.direction = 1;
    action.speed_permille = 800u;
    action.max_run_ms = 0u;
    action.max_action_pulses = 0u;
    action.config_version = 3u;

    CHECK(fa_rs485_master_build_start_action(&master, 7u, &action, request, sizeof(request), &request_len, &seq) == FA_FRAME_OK);
    CHECK(transact(&station, request, request_len, response, sizeof(response), &response_len) == 0);
    CHECK(fa_rs485_master_parse_common(response, response_len, 7u, seq, FA_CMD_START_ACTION, &common) == FA_STATUS_OK);
    CHECK(common.status_code == FA_STATUS_OK);

    FaMasterStatusResponse status;
    for (uint8_t i = 0u; i < 10u; ++i) {
        fake_station_step(&station, 100u);
        CHECK(fa_rs485_master_build_get_status(&master, 7u, request, sizeof(request), &request_len, &seq) == FA_FRAME_OK);
        CHECK(transact(&station, request, request_len, response, sizeof(response), &response_len) == 0);
        CHECK(fa_rs485_master_parse_status(response, response_len, 7u, seq, &status) == FA_STATUS_OK);
        if (status.motor_state == FA_MOTOR_COMPLETED) {
            break;
        }
    }

    CHECK(status.motor_state == FA_MOTOR_COMPLETED);
    CHECK(status.active_action_id == 9001u);
    CHECK(status.completed_pulses >= 1200u);
    CHECK(status.last_stop_reason == FA_STOP_TARGET_REACHED);
    printf("master loop smoke tests passed\n");
    return 0;
}
