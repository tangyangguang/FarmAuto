#include "fa_rs485_master.h"
#include "fa_station_board.h"
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

static void tick_station(FaStationNode *node, uint32_t now_ms) {
    fa_station_board_tick(now_ms);
    fa_station_node_tick(node,
                         now_ms,
                         fa_station_board_position_pulses(),
                         fa_station_board_current_ma());
    fa_station_board_apply_output(&node->output);
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
    fa_station_board_init(0u);
    require_true(fa_station_board_address_input() == 1u, "default simulated address");
    tick_station(&node, 0u);
    fa_rs485_master_init(&master);

    FaMasterMotorConfig config = {
        1u,
        0u,
        4320u,
        800u,
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
        2001u,
        FA_DEVICE_TYPE_FEEDER,
        FA_ACTION_TYPE_FEED,
        FA_TARGET_MODE_RELATIVE_PULSES,
        0,
        1200,
        1,
        800u,
        10000u,
        20000u,
        1u
    };
    require_true(fa_rs485_master_build_start_action(&master, 7u, &action, request, sizeof(request), &request_len, &seq) == FA_FRAME_OK,
                 "build start action");
    response_len = exchange(&node, request, request_len, response, sizeof(response));
    require_true(fa_rs485_master_parse_common(response, response_len, 7u, seq, FA_CMD_START_ACTION, &common) == FA_STATUS_OK,
                 "start response");

    FaMasterStatusResponse status;
    uint8_t saw_running = 0u;
    for (uint32_t now_ms = 0u; now_ms <= 1000u; now_ms += 100u) {
        tick_station(&node, now_ms);
        require_true(fa_rs485_master_build_get_status(&master, 7u, request, sizeof(request), &request_len, &seq) == FA_FRAME_OK,
                     "build status");
        response_len = exchange(&node, request, request_len, response, sizeof(response));
        require_true(fa_rs485_master_parse_status(response, response_len, 7u, seq, &status) == FA_STATUS_OK,
                     "status response");
        if (status.motor_state == FA_MOTOR_RUNNING) {
            saw_running = 1u;
        }
        if (status.motor_state == FA_MOTOR_COMPLETED) {
            break;
        }
    }

    require_true(saw_running != 0u, "station should report running before completion");
    require_true(status.motor_state == FA_MOTOR_COMPLETED, "simulated board should complete action");
    require_true(status.position_pulses >= 1200, "position should advance");
    require_true(status.completed_pulses >= 1200u, "completed pulses should advance");

    printf("station board smoke tests passed\n");
    return 0;
}
