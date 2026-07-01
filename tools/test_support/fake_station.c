#include "fake_station.h"

#include "fa_payload.h"

#include <string.h>

void fake_station_init(FakeStation *station, uint8_t address) {
    memset(station, 0, sizeof(*station));
    station->address = address;
    station->current_ma = 300u;
    fa_action_init(&station->action);
}

void fake_station_step(FakeStation *station, uint32_t delta_ms) {
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

    uint32_t pulses_per_turn = 0u;
    uint16_t default_speed_permille = 0u;
    uint16_t accel_ms = 0u;
    uint16_t decel_ms = 0u;

    if (fa_payload_read_u16(&reader, &config.config_version) != FA_PAYLOAD_OK ||
        fa_payload_read_u16(&reader, &config.flags) != FA_PAYLOAD_OK ||
        fa_payload_read_u32(&reader, &pulses_per_turn) != FA_PAYLOAD_OK ||
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
                                      FA_CAP_CONFIG_REQUIRED_AFTER_BOOT |
                                      FA_CAP_CLEAR_FAULT_SUPPORTED);
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

FaFrameResult fake_station_handle(FakeStation *station, const uint8_t *request_data, size_t request_len, uint8_t *response_data, size_t response_cap, size_t *response_len) {
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
        case FA_CMD_STOP_ACTION:
            fa_action_request_stop(&station->action, station->now_ms);
            write_common_response(&writer, FA_STATUS_OK, station);
            break;
        case FA_CMD_CLEAR_FAULT:
            fa_action_clear_fault(&station->action);
            write_common_response(&writer, FA_STATUS_OK, station);
            break;
        default:
            response.flags |= FA_FRAME_FLAG_ERROR;
            write_common_response(&writer, FA_STATUS_ERR_BAD_PARAM, station);
            break;
    }

    response.len = (uint8_t)fa_payload_writer_len(&writer);
    return fa_frame_encode(&response, response_data, response_cap, response_len);
}
