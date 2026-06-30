#include "fa_feed_service.h"
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

static FaFeedDeviceConfig default_feed_config(void) {
    FaFeedDeviceConfig config;
    config.station_address = 7u;
    config.config_version = 3u;
    config.pulses_per_turn = 4320u;
    config.grams_per_turn_mg = 8000u;
    config.feed_direction = 1;
    config.speed_permille = 800u;
    config.accel_ms = 1000u;
    config.decel_ms = 500u;
    config.over_current_ma = 2000u;
    config.over_current_hold_ms = 100u;
    config.stall_detect_ms = 500u;
    config.stall_min_delta_pulses = 10u;
    config.max_run_ms = 60000u;
    config.max_action_pulses = 432000u;
    return config;
}

static int test_calculation(void) {
    FaFeedDeviceConfig config = default_feed_config();
    uint32_t pulses = 0u;
    CHECK(fa_feed_calculate_target_pulses(&config, FA_FEED_AMOUNT_TURNS_X1000, 1000u, &pulses) == FA_STATUS_OK);
    CHECK(pulses == 4320u);
    CHECK(fa_feed_calculate_target_pulses(&config, FA_FEED_AMOUNT_MG, 8000u, &pulses) == FA_STATUS_OK);
    CHECK(pulses == 4320u);
    CHECK(fa_feed_calculate_target_pulses(&config, FA_FEED_AMOUNT_MG, 4000u, &pulses) == FA_STATUS_OK);
    CHECK(pulses == 2160u);
    return 0;
}

static int test_manual_feed_loop(void) {
    FaRs485Master master;
    fa_rs485_master_init(&master);

    FaFeedService feed;
    fa_feed_service_init(&feed, 9001u);

    FakeStation station;
    fake_station_init(&station, 7u);

    FaFeedDeviceConfig feed_config = default_feed_config();
    FaMasterMotorConfig motor_config;
    CHECK(fa_feed_make_motor_config(&feed_config, &motor_config) == FA_STATUS_OK);

    uint8_t request[FA_MAX_FRAME_LEN];
    uint8_t response[FA_MAX_FRAME_LEN];
    size_t request_len = 0u;
    size_t response_len = 0u;
    uint8_t seq = 0u;

    CHECK(fa_rs485_master_build_set_motor_config(&master, feed_config.station_address, &motor_config, request, sizeof(request), &request_len, &seq) == FA_FRAME_OK);
    CHECK(transact(&station, request, request_len, response, sizeof(response), &response_len) == 0);
    FaMasterCommonResponse common;
    CHECK(fa_rs485_master_parse_common(response, response_len, feed_config.station_address, seq, FA_CMD_SET_MOTOR_CONFIG, &common) == FA_STATUS_OK);
    CHECK(common.status_code == FA_STATUS_OK);

    FaMasterActionRequest action;
    FaFeedResult result;
    CHECK(fa_feed_make_manual_action(&feed, &feed_config, FA_FEED_AMOUNT_MG, 4000u, &action, &result) == FA_STATUS_OK);
    CHECK(action.target_pulses == 2160);
    CHECK(result.action_id == 9001u);
    CHECK(result.target_pulses == 2160u);

    CHECK(fa_rs485_master_build_start_action(&master, feed_config.station_address, &action, request, sizeof(request), &request_len, &seq) == FA_FRAME_OK);
    CHECK(transact(&station, request, request_len, response, sizeof(response), &response_len) == 0);
    CHECK(fa_rs485_master_parse_common(response, response_len, feed_config.station_address, seq, FA_CMD_START_ACTION, &common) == FA_STATUS_OK);
    CHECK(common.status_code == FA_STATUS_OK);

    FaMasterStatusResponse status;
    for (uint8_t i = 0u; i < 20u; ++i) {
        fake_station_step(&station, 100u);
        CHECK(fa_rs485_master_build_get_status(&master, feed_config.station_address, request, sizeof(request), &request_len, &seq) == FA_FRAME_OK);
        CHECK(transact(&station, request, request_len, response, sizeof(response), &response_len) == 0);
        CHECK(fa_rs485_master_parse_status(response, response_len, feed_config.station_address, seq, &status) == FA_STATUS_OK);
        CHECK(fa_feed_result_from_status(&status, &result) == FA_STATUS_OK);
        if (result.completed != 0u || result.failed != 0u) {
            break;
        }
    }

    CHECK(result.completed == 1u);
    CHECK(result.failed == 0u);
    CHECK(result.stop_reason == FA_STOP_TARGET_REACHED);
    CHECK(result.completed_pulses >= result.target_pulses);
    return 0;
}

int main(void) {
    CHECK(test_calculation() == 0);
    CHECK(test_manual_feed_loop() == 0);
    printf("feed service smoke tests passed\n");
    return 0;
}
