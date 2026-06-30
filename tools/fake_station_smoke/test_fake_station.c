#include "fake_station.h"
#include "fa_payload.h"
#include "fa_protocol.h"

#include <stdio.h>
#include <string.h>

#define CHECK(expr)                                                       \
    do {                                                                  \
        if (!(expr)) {                                                    \
            printf("CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #expr); \
            return 1;                                                     \
        }                                                                 \
    } while (0)

static int build_set_config(uint8_t seq, uint8_t dst, uint8_t *out, size_t out_cap, size_t *out_len) {
    FaFrame frame;
    memset(&frame, 0, sizeof(frame));
    frame.version = FA_PROTOCOL_VERSION;
    frame.dst = dst;
    frame.src = FA_MASTER_ADDRESS;
    frame.seq = seq;
    frame.cmd = FA_CMD_SET_MOTOR_CONFIG;

    FaPayloadWriter writer;
    fa_payload_writer_init(&writer, frame.payload, sizeof(frame.payload));
    CHECK(fa_payload_write_u16(&writer, 3u) == FA_PAYLOAD_OK);
    CHECK(fa_payload_write_u16(&writer, 0u) == FA_PAYLOAD_OK);
    CHECK(fa_payload_write_u32(&writer, 4320u) == FA_PAYLOAD_OK);
    CHECK(fa_payload_write_u16(&writer, 800u) == FA_PAYLOAD_OK);
    CHECK(fa_payload_write_u16(&writer, 1000u) == FA_PAYLOAD_OK);
    CHECK(fa_payload_write_u16(&writer, 500u) == FA_PAYLOAD_OK);
    CHECK(fa_payload_write_u16(&writer, 2000u) == FA_PAYLOAD_OK);
    CHECK(fa_payload_write_u16(&writer, 100u) == FA_PAYLOAD_OK);
    CHECK(fa_payload_write_u16(&writer, 500u) == FA_PAYLOAD_OK);
    CHECK(fa_payload_write_u16(&writer, 10u) == FA_PAYLOAD_OK);
    CHECK(fa_payload_write_u32(&writer, 60000u) == FA_PAYLOAD_OK);
    CHECK(fa_payload_write_u32(&writer, 432000u) == FA_PAYLOAD_OK);
    frame.len = (uint8_t)fa_payload_writer_len(&writer);

    CHECK(fa_frame_encode(&frame, out, out_cap, out_len) == FA_FRAME_OK);
    return 0;
}

static int build_start_action(uint8_t seq, uint8_t dst, uint8_t *out, size_t out_cap, size_t *out_len) {
    FaFrame frame;
    memset(&frame, 0, sizeof(frame));
    frame.version = FA_PROTOCOL_VERSION;
    frame.dst = dst;
    frame.src = FA_MASTER_ADDRESS;
    frame.seq = seq;
    frame.cmd = FA_CMD_START_ACTION;

    FaPayloadWriter writer;
    fa_payload_writer_init(&writer, frame.payload, sizeof(frame.payload));
    CHECK(fa_payload_write_u32(&writer, 9001u) == FA_PAYLOAD_OK);
    CHECK(fa_payload_write_u8(&writer, FA_DEVICE_TYPE_FEEDER) == FA_PAYLOAD_OK);
    CHECK(fa_payload_write_u8(&writer, FA_ACTION_TYPE_FEED) == FA_PAYLOAD_OK);
    CHECK(fa_payload_write_u8(&writer, FA_TARGET_MODE_RELATIVE_PULSES) == FA_PAYLOAD_OK);
    CHECK(fa_payload_write_i32(&writer, 0) == FA_PAYLOAD_OK);
    CHECK(fa_payload_write_i32(&writer, 1200) == FA_PAYLOAD_OK);
    CHECK(fa_payload_write_i8(&writer, 1) == FA_PAYLOAD_OK);
    CHECK(fa_payload_write_u16(&writer, 800u) == FA_PAYLOAD_OK);
    CHECK(fa_payload_write_u32(&writer, 0u) == FA_PAYLOAD_OK);
    CHECK(fa_payload_write_u32(&writer, 0u) == FA_PAYLOAD_OK);
    CHECK(fa_payload_write_u16(&writer, 3u) == FA_PAYLOAD_OK);
    frame.len = (uint8_t)fa_payload_writer_len(&writer);

    CHECK(fa_frame_encode(&frame, out, out_cap, out_len) == FA_FRAME_OK);
    return 0;
}

static int build_get_status(uint8_t seq, uint8_t dst, uint8_t *out, size_t out_cap, size_t *out_len) {
    FaFrame frame;
    memset(&frame, 0, sizeof(frame));
    frame.version = FA_PROTOCOL_VERSION;
    frame.dst = dst;
    frame.src = FA_MASTER_ADDRESS;
    frame.seq = seq;
    frame.cmd = FA_CMD_GET_STATUS;
    frame.len = 0u;
    CHECK(fa_frame_encode(&frame, out, out_cap, out_len) == FA_FRAME_OK);
    return 0;
}

static int read_common_status(const uint8_t *response, size_t response_len, uint8_t expected_cmd, uint8_t *motor_state, uint32_t *completed_pulses) {
    FaFrame frame;
    CHECK(fa_frame_decode(response, response_len, &frame) == FA_FRAME_OK);
    CHECK(frame.cmd == expected_cmd);

    FaPayloadReader reader;
    fa_payload_reader_init(&reader, frame.payload, frame.len);
    uint8_t status_code = 0u;
    uint8_t station_state = 0u;
    uint16_t fault_code = 0u;
    CHECK(fa_payload_read_u8(&reader, &status_code) == FA_PAYLOAD_OK);
    CHECK(fa_payload_read_u8(&reader, &station_state) == FA_PAYLOAD_OK);
    CHECK(fa_payload_read_u16(&reader, &fault_code) == FA_PAYLOAD_OK);
    CHECK(status_code == FA_STATUS_OK);
    CHECK(fault_code == FA_FAULT_NONE);

    if (expected_cmd == FA_CMD_GET_STATUS) {
        uint32_t action_id = 0u;
        int32_t pos = 0;
        int32_t target = 0;
        uint16_t current = 0u;
        uint16_t peak = 0u;
        uint32_t run_ms = 0u;
        uint8_t stop_reason = 0u;
        CHECK(fa_payload_read_u8(&reader, motor_state) == FA_PAYLOAD_OK);
        CHECK(fa_payload_read_u32(&reader, &action_id) == FA_PAYLOAD_OK);
        CHECK(fa_payload_read_i32(&reader, &pos) == FA_PAYLOAD_OK);
        CHECK(fa_payload_read_i32(&reader, &target) == FA_PAYLOAD_OK);
        CHECK(fa_payload_read_u16(&reader, &current) == FA_PAYLOAD_OK);
        CHECK(fa_payload_read_u16(&reader, &peak) == FA_PAYLOAD_OK);
        CHECK(fa_payload_read_u32(&reader, &run_ms) == FA_PAYLOAD_OK);
        CHECK(fa_payload_read_u32(&reader, completed_pulses) == FA_PAYLOAD_OK);
        CHECK(fa_payload_read_u8(&reader, &stop_reason) == FA_PAYLOAD_OK);
        CHECK(action_id == 9001u);
        CHECK(target == 1200);
    }
    return 0;
}

int main(void) {
    FakeStation station;
    fake_station_init(&station, 7u);

    uint8_t request[FA_MAX_FRAME_LEN];
    uint8_t response[FA_MAX_FRAME_LEN];
    size_t request_len = 0u;
    size_t response_len = 0u;

    CHECK(build_set_config(1u, 7u, request, sizeof(request), &request_len) == 0);
    CHECK(fake_station_handle(&station, request, request_len, response, sizeof(response), &response_len) == FA_FRAME_OK);
    CHECK(read_common_status(response, response_len, FA_CMD_SET_MOTOR_CONFIG, NULL, NULL) == 0);

    CHECK(build_start_action(2u, 7u, request, sizeof(request), &request_len) == 0);
    CHECK(fake_station_handle(&station, request, request_len, response, sizeof(response), &response_len) == FA_FRAME_OK);
    CHECK(read_common_status(response, response_len, FA_CMD_START_ACTION, NULL, NULL) == 0);

    uint8_t motor_state = FA_MOTOR_UNCONFIGURED;
    uint32_t completed_pulses = 0u;
    for (uint8_t i = 0u; i < 10u; ++i) {
        fake_station_step(&station, 100u);
        CHECK(build_get_status((uint8_t)(3u + i), 7u, request, sizeof(request), &request_len) == 0);
        CHECK(fake_station_handle(&station, request, request_len, response, sizeof(response), &response_len) == FA_FRAME_OK);
        CHECK(read_common_status(response, response_len, FA_CMD_GET_STATUS, &motor_state, &completed_pulses) == 0);
        if (motor_state == FA_MOTOR_COMPLETED) {
            break;
        }
    }

    CHECK(motor_state == FA_MOTOR_COMPLETED);
    CHECK(completed_pulses >= 1200u);
    printf("fake station smoke tests passed\n");
    return 0;
}
