#include "fa_action_record.h"
#include "fa_feed_service.h"
#include "fake_station.h"

#include <stdio.h>
#include <string.h>

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

int main(void) {
    FaRs485Master master;
    FaFeedService feed;
    FakeStation station;
    FaFeedDeviceConfig feed_config = default_feed_config();
    FaMasterMotorConfig motor_config;
    FaMasterActionRequest action;
    FaFeedResult feed_result;
    FaActionRecord record;
    uint8_t request[FA_MAX_FRAME_LEN];
    uint8_t response[FA_MAX_FRAME_LEN];
    size_t request_len = 0u;
    size_t response_len = 0u;
    uint8_t seq = 0u;

    fa_rs485_master_init(&master);
    fa_feed_service_init(&feed, 9001u);
    fake_station_init(&station, feed_config.station_address);

    CHECK(fa_feed_make_motor_config(&feed_config, &motor_config) == FA_STATUS_OK);
    CHECK(fa_rs485_master_build_set_motor_config(&master, feed_config.station_address, &motor_config, request, sizeof(request), &request_len, &seq) == FA_FRAME_OK);
    CHECK(transact(&station, request, request_len, response, sizeof(response), &response_len) == 0);

    CHECK(fa_feed_make_manual_action(&feed, &feed_config, FA_FEED_AMOUNT_MG, 4000u, &action, &feed_result) == FA_STATUS_OK);
    FaActionRecordStart start;
    memset(&start, 0, sizeof(start));
    start.action_id = action.action_id;
    start.device_id = 12u;
    strncpy(start.device_name, "Feeder Test", sizeof(start.device_name) - 1u);
    start.bus_address = feed_config.station_address;
    start.device_type = action.device_type;
    start.action_type = action.action_type;
    start.source_type = FA_ACTION_RECORD_SOURCE_MANUAL;
    start.source_id = 0u;
    start.target_pulses = feed_result.target_pulses;
    start.amount_mode = FA_FEED_AMOUNT_MG;
    start.amount_value = 4000u;
    start.started_at_s = 100u;
    CHECK(fa_action_record_begin(&record, &start) == FA_STATUS_OK);
    CHECK(record.state == FA_ACTION_RECORD_RUNNING);
    CHECK(strcmp(record.device_name, "Feeder Test") == 0);

    CHECK(fa_rs485_master_build_start_action(&master, feed_config.station_address, &action, request, sizeof(request), &request_len, &seq) == FA_FRAME_OK);
    CHECK(transact(&station, request, request_len, response, sizeof(response), &response_len) == 0);

    FaMasterStatusResponse status;
    for (uint8_t i = 0u; i < 20u; ++i) {
        fake_station_step(&station, 100u);
        CHECK(fa_rs485_master_build_get_status(&master, feed_config.station_address, request, sizeof(request), &request_len, &seq) == FA_FRAME_OK);
        CHECK(transact(&station, request, request_len, response, sizeof(response), &response_len) == 0);
        CHECK(fa_rs485_master_parse_status(response, response_len, feed_config.station_address, seq, &status) == FA_STATUS_OK);
        CHECK(fa_action_record_apply_status(&record, &status, 101u + i) == FA_STATUS_OK);
        if (fa_action_record_is_terminal(&record) != 0u) {
            break;
        }
    }

    CHECK(record.state == FA_ACTION_RECORD_COMPLETED);
    CHECK(record.ended_at_s >= record.started_at_s);
    CHECK(record.completed_pulses >= record.target_pulses);
    CHECK(record.stop_reason == FA_STOP_TARGET_REACHED);
    CHECK(record.fault_code == FA_FAULT_NONE);
    CHECK(record.bus_address == feed_config.station_address);
    CHECK(record.device_id == 12u);

    uint8_t encoded[FA_ACTION_RECORD_ENCODED_LEN];
    size_t encoded_len = 0u;
    FaActionRecord decoded;
    CHECK(fa_action_record_encode(&record, encoded, sizeof(encoded), &encoded_len) == FA_STATUS_OK);
    CHECK(encoded_len == FA_ACTION_RECORD_ENCODED_LEN);
    CHECK(fa_action_record_decode(encoded, encoded_len, &decoded) == FA_STATUS_OK);
    CHECK(decoded.action_id == record.action_id);
    CHECK(decoded.device_id == record.device_id);
    CHECK(strcmp(decoded.device_name, "Feeder Test") == 0);
    CHECK(decoded.bus_address == record.bus_address);
    CHECK(decoded.target_pulses == record.target_pulses);
    CHECK(decoded.completed_pulses == record.completed_pulses);
    CHECK(decoded.stop_reason == record.stop_reason);
    CHECK(decoded.state == record.state);

    printf("action record smoke tests passed\n");
    return 0;
}
