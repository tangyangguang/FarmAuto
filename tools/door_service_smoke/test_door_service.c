#include "fa_door_service.h"
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

static FaDoorDeviceConfig default_door_config(void) {
    FaDoorDeviceConfig config;
    config.station_address = 3u;
    config.config_version = 2u;
    config.pulses_per_turn = 4320u;
    config.travel_pulses = 1200u;
    config.open_direction = 1;
    config.close_direction = -1;
    config.speed_permille = 700u;
    config.accel_ms = 0u;
    config.decel_ms = 0u;
    config.over_current_ma = 2500u;
    config.over_current_hold_ms = 100u;
    config.stall_detect_ms = 500u;
    config.stall_min_delta_pulses = 10u;
    config.max_run_ms = 30000u;
    config.max_action_pulses = 10000u;
    return config;
}

int main(void) {
    FaRs485Master master;
    FaDoorService door;
    FakeStation station;
    FaDoorDeviceConfig config = default_door_config();
    FaMasterMotorConfig motor_config;
    FaMasterActionRequest action;
    FaDoorResult result;
    uint8_t request[FA_MAX_FRAME_LEN];
    uint8_t response[FA_MAX_FRAME_LEN];
    size_t request_len = 0u;
    size_t response_len = 0u;
    uint8_t seq = 0u;

    fa_rs485_master_init(&master);
    fa_door_service_init(&door, 700001u);
    fake_station_init(&station, config.station_address);

    CHECK(fa_door_make_motor_config(&config, &motor_config) == FA_STATUS_OK);
    CHECK(fa_rs485_master_build_set_motor_config(&master, config.station_address, &motor_config, request, sizeof(request), &request_len, &seq) == FA_FRAME_OK);
    CHECK(transact(&station, request, request_len, response, sizeof(response), &response_len) == 0);

    CHECK(fa_door_make_action(&door, &config, FA_DOOR_COMMAND_OPEN, &action, &result) == FA_STATUS_OK);
    CHECK(action.action_id == 700001u);
    CHECK(action.device_type == FA_DEVICE_TYPE_DOOR);
    CHECK(action.action_type == FA_ACTION_TYPE_DOOR_OPEN);
    CHECK(action.direction == 1);
    CHECK(action.target_pulses == 1200);
    CHECK(result.target_pulses == 1200u);

    CHECK(fa_rs485_master_build_start_action(&master, config.station_address, &action, request, sizeof(request), &request_len, &seq) == FA_FRAME_OK);
    CHECK(transact(&station, request, request_len, response, sizeof(response), &response_len) == 0);

    fake_station_step(&station, 100u);
    CHECK(fa_rs485_master_build_stop_action(&master, config.station_address, request, sizeof(request), &request_len, &seq) == FA_FRAME_OK);
    CHECK(transact(&station, request, request_len, response, sizeof(response), &response_len) == 0);
    FaMasterCommonResponse common;
    CHECK(fa_rs485_master_parse_common(response, response_len, config.station_address, seq, FA_CMD_STOP_ACTION, &common) == FA_STATUS_OK);

    CHECK(fa_door_make_action(&door, &config, FA_DOOR_COMMAND_CLOSE, &action, &result) == FA_STATUS_OK);
    CHECK(action.action_id == 700002u);
    CHECK(action.action_type == FA_ACTION_TYPE_DOOR_CLOSE);
    CHECK(action.direction == -1);

    printf("door service smoke tests passed\n");
    return 0;
}
