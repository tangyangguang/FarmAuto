#include "fa_action_controller.h"
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

typedef struct {
    uint8_t address;
    uint32_t now_ms;
    int32_t position_pulses;
    uint16_t current_ma;
    FaActionController action;
    FaActionOutput last_output;
} FakeStation;

static void fake_station_init(FakeStation *station, uint8_t address) {
    memset(station, 0, sizeof(*station));
    station->address = address;
    station->current_ma = 300u;
    fa_action_init(&station->action);
}

static void fake_station_step(FakeStation *station, uint32_t delta_ms) {
    station->now_ms += delta_ms;

    if (station->last_output.motor_enable != 0u) {
        const int32_t delta = (int32_t)(delta_ms * 3u);
        station->position_pulses += station->last_output.direction >= 0 ? delta : -delta;
    }

    FaActionInputs inputs = {station->now_ms, station->position_pulses, station->current_ma};
    fa_action_tick(&station->action, &inputs, &station->last_output);
}

static void write_common_response(FaPayloadWriter *writer, uint8_t status_code, const FakeStation *station) {
    FaActionInputs inputs = {station->now_ms, station->position_pulses, station->current_ma};
    FaActionStatus status;
    fa_action_get_status(&station->action, &inputs, &status);

    (void)fa_payload_write_u8(writer, status_code);
    (void)fa_payload_write_u8(writer, status.motor_state == FA_MOTOR_FAULT ? FA_STATION_FAULT : FA_STATION_READY);
    (void)fa_payload_write_u16(writer, status.fault_code);
}

static uint8_t handle_set_motor_config(FakeStation *station, const FaFrame *request) {
    FaPayloadReader reader;
    fa_payload_reader_init(&reader, request->payload, request->len);

    FaActionConfig config;
    memset(&config, 0, sizeof(config));

    if (fa_payload_read_u16(&reader, &config.config_version) != FA_PAYLOAD_OK ||
        fa_payload_read_u16(&reader, &config.flags) != FA_PAYLOAD_OK) {
        return FA_STATUS_ERR_BAD_LENGTH;
    }

    uint32_t pulses_per_turn = 0u;
    uint16_t default_speed_permille = 0u;
    uint16_t accel_ms = 0u;
    uint16_t decel_ms = 0u;
    if (fa_payload_read_u32(&reader, &pulses_per_turn) != FA_PAYLOAD_OK ||
        fa_payload_read_u16(&reader, &default_speed_permille) != FA_PAYLOAD_OK ||
        fa_payload_read_u16(&reader, &accel_ms) != FA_PAYLOAD_OK ||
        fa_payload_read_u16(&reader, &decel_ms) != FA_PAYLOAD_OK ||
        fa_payload_read_u16(&reader, &config.over_current_ma) != FA_PAYLOAD_OK ||
        fa_payload_read_u16(&reader, &config.over_current_hold_ms) != FA_PAYLOAD_OK ||
        fa_payload_read_u16(&reader, &config.stall_detect_ms) != FA_PAYLOAD_OK ||
        fa_payload_read_u16(&reader, &config.stall_min_delta_pulses) != FA_PAYLOAD_OK ||
        fa_payload_read_u32(&reader, &config.max_run_ms) != FA_PAYLOAD_OK ||
        fa_payload_read_u32(&reader, &config.max_action_pulses) != FA_PAYLOAD_OK) {
        return FA_STATUS_ERR_BAD_LENGTH;
    }
    (void)pulses_per_turn;
    (void)default_speed_permille;
    (void)accel_ms;
    (void)decel_ms;

    return fa_action_configure(&station->action, &config);
}

static uint8_t handle_start_action(FakeStation *station, const FaFrame *request) {
    FaPayloadReader reader;
    fa_payload_reader_init(&reader, request->payload, request->len);

    FaActionRequest action;
    memset(&action, 0, sizeof(action));

    if (fa_payload_read_u32(&reader, &action.action_id) != FA_PAYLOAD_OK ||
        fa_payload_read_u8(&reader, &action.device_type) != FA_PAYLOAD_OK ||
        fa_payload_read_u8(&reader, &action.action_type) != FA_PAYLOAD_OK ||
        fa_payload_read_u8(&reader, &action.target_mode) != FA_PAYLOAD_OK ||
        fa_payload_read_i32(&reader, &action.start_position_pulses) != FA_PAYLOAD_OK ||
        fa_payload_read_i32(&reader, &action.target_pulses) != FA_PAYLOAD_OK ||
        fa_payload_read_i8(&reader, &action.direction) != FA_PAYLOAD_OK ||
        fa_payload_read_u16(&reader, &action.speed_permille) != FA_PAYLOAD_OK ||
        fa_payload_read_u32(&reader, &action.max_run_ms) != FA_PAYLOAD_OK ||
        fa_payload_read_u32(&reader, &action.max_action_pulses) != FA_PAYLOAD_OK ||
        fa_payload_read_u16(&reader, &action.config_version) != FA_PAYLOAD_OK) {
        return FA_STATUS_ERR_BAD_LENGTH;
    }

    return fa_action_start(&station->action, &action, station->now_ms);
}

static void write_ping_payload(FaPayloadWriter *writer, const FakeStation *station) {
    write_common_response(writer, FA_STATUS_OK, station);
    (void)fa_payload_write_u8(writer, FA_PROTOCOL_VERSION);
    (void)fa_payload_write_u16(writer, 1u);
    (void)fa_payload_write_u8(writer, station->address);
    (void)fa_payload_write_u8(writer, station->address);
    (void)fa_payload_write_u8(writer, FA_DEVICE_CLASS_MOTOR_ACTUATOR);
    (void)fa_payload_write_u32(writer, FA_CAP_MOTOR_BIDIRECTIONAL |
                                      FA_CAP_HALL_AB_ENCODER |
                                      FA_CAP_CURRENT_SENSE |
                                      FA_CAP_BRAKE_SUPPORTED |
                                      FA_CAP_CONFIG_REQUIRED_AFTER_BOOT);
    (void)fa_payload_write_u8(writer, FA_MAX_PAYLOAD_LEN);
}

static void write_status_payload(FaPayloadWriter *writer, const FakeStation *station) {
    FaActionInputs inputs = {station->now_ms, station->position_pulses, station->current_ma};
    FaActionStatus status;
    fa_action_get_status(&station->action, &inputs, &status);

    write_common_response(writer, FA_STATUS_OK, station);
    (void)fa_payload_write_u8(writer, status.motor_state);
    (void)fa_payload_write_u32(writer, status.active_action_id);
    (void)fa_payload_write_i32(writer, status.position_pulses);
    (void)fa_payload_write_i32(writer, status.target_pulses);
    (void)fa_payload_write_u16(writer, status.current_ma);
    (void)fa_payload_write_u16(writer, status.peak_current_ma);
    (void)fa_payload_write_u32(writer, status.run_ms);
    (void)fa_payload_write_u32(writer, status.completed_pulses);
    (void)fa_payload_write_u8(writer, status.last_stop_reason);
}

static FaFrameResult fake_station_handle(FakeStation *station, const uint8_t *request_data, size_t request_len, uint8_t *response_data, size_t response_cap, size_t *response_len) {
    FaFrame request;
    FaFrameResult decode_result = fa_frame_decode(request_data, request_len, &request);
    if (decode_result != FA_FRAME_OK) {
        return decode_result;
    }
    if (request.dst != station->address) {
        return FA_FRAME_ERR_BAD_SOF;
    }

    FaFrame response;
    memset(&response, 0, sizeof(response));
    response.version = FA_PROTOCOL_VERSION;
    response.flags = FA_FRAME_FLAG_RESPONSE;
    response.dst = request.src;
    response.src = station->address;
    response.seq = request.seq;
    response.cmd = request.cmd;

    FaPayloadWriter writer;
    fa_payload_writer_init(&writer, response.payload, sizeof(response.payload));

    uint8_t status_code = FA_STATUS_OK;
    switch (request.cmd) {
        case FA_CMD_PING:
            write_ping_payload(&writer, station);
            break;
        case FA_CMD_SET_MOTOR_CONFIG:
            status_code = handle_set_motor_config(station, &request);
            if (status_code != FA_STATUS_OK) {
                response.flags |= FA_FRAME_FLAG_ERROR;
            }
            write_common_response(&writer, status_code, station);
            break;
        case FA_CMD_START_ACTION:
            status_code = handle_start_action(station, &request);
            if (status_code != FA_STATUS_OK) {
                response.flags |= FA_FRAME_FLAG_ERROR;
            }
            write_common_response(&writer, status_code, station);
            break;
        case FA_CMD_GET_STATUS:
            write_status_payload(&writer, station);
            break;
        default:
            response.flags |= FA_FRAME_FLAG_ERROR;
            write_common_response(&writer, FA_STATUS_ERR_BAD_PARAM, station);
            break;
    }

    response.len = (uint8_t)fa_payload_writer_len(&writer);
    return fa_frame_encode(&response, response_data, response_cap, response_len);
}

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
