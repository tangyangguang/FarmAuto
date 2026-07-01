#include "fa_rs485_master.h"
#include "fa_station_node.h"

#include <stdio.h>
#include <stdlib.h>

static void require_true(int condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
        exit(1);
    }
}

static size_t exchange(FaStationNode *node, const uint8_t *request, size_t request_len, uint8_t *response, size_t response_cap) {
    size_t response_len = 0u;
    for (size_t i = 0u; i < request_len; ++i) {
        FaFrameResult result = fa_station_node_push_byte(node, request[i], response, response_cap, &response_len);
        if (i + 1u < request_len) {
            require_true(result == FA_FRAME_INCOMPLETE, "partial frame should remain incomplete");
        } else {
            require_true(result == FA_FRAME_OK, "complete frame should produce response");
            require_true(response_len > 0u, "response should not be empty");
        }
    }
    return response_len;
}

int main(void) {
    FaStationNode node;
    FaRs485Master master;
    uint8_t request[FA_MAX_FRAME_LEN];
    uint8_t response[FA_MAX_FRAME_LEN];
    size_t request_len = 0u;
    size_t response_len = 0u;
    uint8_t seq = 0u;

    fa_station_node_init(&node, 7u);
    fa_station_node_tick(&node, 0u, 0, 120u);
    fa_rs485_master_init(&master);

    require_true(fa_rs485_master_build_ping(&master, 7u, request, sizeof(request), &request_len, &seq) == FA_FRAME_OK,
                 "build ping");
    response_len = exchange(&node, request, request_len, response, sizeof(response));
    FaMasterPingResponse ping;
    require_true(fa_rs485_master_parse_ping(response, response_len, 7u, seq, &ping) == FA_STATUS_OK,
                 "parse ping");
    require_true(ping.device_class == FA_DEVICE_CLASS_MOTOR_ACTUATOR, "station class");
    require_true((ping.capability_flags & FA_CAP_MOTOR_BIDIRECTIONAL) != 0u, "bidirectional capability");

    FaMasterMotorConfig config = {
        1u,
        0u,
        4320u,
        650u,
        0u,
        0u,
        1800u,
        200u,
        1000u,
        3u,
        10000u,
        20000u
    };
    require_true(fa_rs485_master_build_set_motor_config(&master, 7u, &config, request, sizeof(request), &request_len, &seq) == FA_FRAME_OK,
                 "build motor config");
    response_len = exchange(&node, request, request_len, response, sizeof(response));
    FaMasterCommonResponse common;
    require_true(fa_rs485_master_parse_common(response, response_len, 7u, seq, FA_CMD_SET_MOTOR_CONFIG, &common) == FA_STATUS_OK,
                 "config response");

    FaMasterActionRequest action = {
        1001u,
        FA_DEVICE_TYPE_FEEDER,
        FA_ACTION_TYPE_FEED,
        FA_TARGET_MODE_RELATIVE_PULSES,
        0,
        20,
        1,
        650u,
        10000u,
        20000u,
        1u
    };
    require_true(fa_rs485_master_build_start_action(&master, 7u, &action, request, sizeof(request), &request_len, &seq) == FA_FRAME_OK,
                 "build start action");
    response_len = exchange(&node, request, request_len, response, sizeof(response));
    require_true(fa_rs485_master_parse_common(response, response_len, 7u, seq, FA_CMD_START_ACTION, &common) == FA_STATUS_OK,
                 "start response");

    fa_station_node_tick(&node, 1000u, 20, 200u);
    require_true(fa_rs485_master_build_get_status(&master, 7u, request, sizeof(request), &request_len, &seq) == FA_FRAME_OK,
                 "build status");
    response_len = exchange(&node, request, request_len, response, sizeof(response));
    FaMasterStatusResponse status;
    require_true(fa_rs485_master_parse_status(response, response_len, 7u, seq, &status) == FA_STATUS_OK,
                 "status response");
    require_true(status.motor_state == FA_MOTOR_COMPLETED, "action should complete");
    require_true(status.completed_pulses == 20u, "completed pulses");
    require_true(status.run_ms == 1000u, "completed run time");

    require_true(fa_rs485_master_build_clear_fault(&master, 7u, request, sizeof(request), &request_len, &seq) == FA_FRAME_OK,
                 "build clear fault");
    response_len = exchange(&node, request, request_len, response, sizeof(response));
    require_true(fa_rs485_master_parse_common(response, response_len, 7u, seq, FA_CMD_CLEAR_FAULT, &common) == FA_STATUS_OK,
                 "clear fault response");
    require_true(common.status_code == FA_STATUS_OK, "clear fault accepted");

    printf("station node smoke tests passed\n");
    return 0;
}
