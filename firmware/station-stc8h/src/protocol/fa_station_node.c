#include "fa_station_node.h"

#include "fa_payload.h"

#include <string.h>

static void station_write_common(FaStationNode *node, FaPayloadWriter *writer, uint8_t status_code) {
    FaActionInputs inputs;
    FaActionStatus status;

    inputs.now_ms = node->now_ms;
    inputs.position_pulses = node->position_pulses;
    inputs.current_ma = node->current_ma;
    fa_action_get_status(&node->action, &inputs, &status);

    (void)fa_payload_write_u8(writer, status_code);
    (void)fa_payload_write_u8(writer, status.motor_state == FA_MOTOR_FAULT ? FA_STATION_FAULT : FA_STATION_READY);
    (void)fa_payload_write_u16(writer, status.fault_code);
}

static uint8_t station_handle_motor_config(FaStationNode *node, const FaFrame *request) {
    FaPayloadReader reader;
    FaActionConfig config;
    uint32_t pulses_per_turn;
    uint16_t default_speed_permille;
    uint16_t accel_ms;
    uint16_t decel_ms;

    fa_payload_reader_init(&reader, request->payload, request->len);
    memset(&config, 0, sizeof(config));

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
    return fa_action_configure(&node->action, &config);
}

static uint8_t station_handle_start_action(FaStationNode *node, const FaFrame *request) {
    FaPayloadReader reader;
    FaActionRequest action;

    fa_payload_reader_init(&reader, request->payload, request->len);
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

    return fa_action_start(&node->action, &action, node->now_ms);
}

static void station_write_ping(FaStationNode *node, FaPayloadWriter *writer) {
    station_write_common(node, writer, FA_STATUS_OK);
    (void)fa_payload_write_u8(writer, FA_PROTOCOL_VERSION);
    (void)fa_payload_write_u16(writer, FA_STATION_FIRMWARE_VERSION);
    (void)fa_payload_write_u8(writer, node->address);
    (void)fa_payload_write_u8(writer, node->raw_address_input);
    (void)fa_payload_write_u8(writer, FA_DEVICE_CLASS_MOTOR_ACTUATOR);
    (void)fa_payload_write_u32(writer, FA_CAP_MOTOR_BIDIRECTIONAL |
                                      FA_CAP_HALL_AB_ENCODER |
                                      FA_CAP_CURRENT_SENSE |
                                      FA_CAP_BRAKE_SUPPORTED |
                                      FA_CAP_CONFIG_REQUIRED_AFTER_BOOT |
                                      FA_CAP_CLEAR_FAULT_SUPPORTED);
    (void)fa_payload_write_u8(writer, FA_MAX_PAYLOAD_LEN);
}

static void station_write_status(FaStationNode *node, FaPayloadWriter *writer) {
    FaActionInputs inputs;
    FaActionStatus status;

    inputs.now_ms = node->now_ms;
    inputs.position_pulses = node->position_pulses;
    inputs.current_ma = node->current_ma;
    fa_action_get_status(&node->action, &inputs, &status);

    station_write_common(node, writer, FA_STATUS_OK);
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

void fa_station_node_init(FaStationNode *node, uint8_t address) {
    if (node == NULL) {
        return;
    }

    memset(node, 0, sizeof(*node));
    fa_action_init(&node->action);
    fa_frame_parser_init(&node->parser);
    fa_station_node_set_address(node, address);
}

void fa_station_node_set_address(FaStationNode *node, uint8_t address) {
    if (node == NULL) {
        return;
    }

    node->raw_address_input = address;
    node->address = fa_address_is_normal(address) ? address : FA_ADDRESS_MIN;
}

void fa_station_node_tick(FaStationNode *node, uint32_t now_ms, int32_t position_pulses, uint16_t current_ma) {
    FaActionInputs inputs;

    if (node == NULL) {
        return;
    }

    node->now_ms = now_ms;
    node->position_pulses = position_pulses;
    node->current_ma = current_ma;

    inputs.now_ms = now_ms;
    inputs.position_pulses = position_pulses;
    inputs.current_ma = current_ma;
    fa_action_tick(&node->action, &inputs, &node->output);
}

FaFrameResult fa_station_node_handle_frame(FaStationNode *node, const FaFrame *request, uint8_t *response, size_t response_cap, size_t *response_len) {
    FaFrame out;
    FaPayloadWriter writer;
    uint8_t status_code;

    if (node == NULL || request == NULL || response == NULL || response_len == NULL) {
        return FA_FRAME_ERR_NULL;
    }
    *response_len = 0u;
    if (request->dst != node->address) {
        return FA_FRAME_INCOMPLETE;
    }

    memset(&out, 0, sizeof(out));
    out.version = FA_PROTOCOL_VERSION;
    out.flags = FA_FRAME_FLAG_RESPONSE;
    out.dst = request->src;
    out.src = node->address;
    out.seq = request->seq;
    out.cmd = request->cmd;

    fa_payload_writer_init(&writer, out.payload, sizeof(out.payload));
    status_code = FA_STATUS_OK;

    switch (request->cmd) {
    case FA_CMD_PING:
        station_write_ping(node, &writer);
        break;
    case FA_CMD_GET_STATUS:
        station_write_status(node, &writer);
        break;
    case FA_CMD_SET_MOTOR_CONFIG:
        status_code = station_handle_motor_config(node, request);
        if (status_code != FA_STATUS_OK) {
            out.flags |= FA_FRAME_FLAG_ERROR;
        }
        station_write_common(node, &writer, status_code);
        break;
    case FA_CMD_START_ACTION:
        status_code = station_handle_start_action(node, request);
        if (status_code != FA_STATUS_OK) {
            out.flags |= FA_FRAME_FLAG_ERROR;
        }
        station_write_common(node, &writer, status_code);
        break;
    case FA_CMD_STOP_ACTION:
        fa_action_request_stop(&node->action, node->now_ms);
        station_write_common(node, &writer, FA_STATUS_OK);
        break;
    case FA_CMD_CLEAR_FAULT:
        fa_action_clear_fault(&node->action);
        station_write_common(node, &writer, FA_STATUS_OK);
        break;
    default:
        out.flags |= FA_FRAME_FLAG_ERROR;
        station_write_common(node, &writer, FA_STATUS_ERR_BAD_PARAM);
        break;
    }

    out.len = (uint8_t)fa_payload_writer_len(&writer);
    return fa_frame_encode(&out, response, response_cap, response_len);
}

FaFrameResult fa_station_node_push_byte(FaStationNode *node, uint8_t byte, uint8_t *response, size_t response_cap, size_t *response_len) {
    FaFrame request;
    FaFrameResult result;

    if (node == NULL || response == NULL || response_len == NULL) {
        return FA_FRAME_ERR_NULL;
    }

    *response_len = 0u;
    result = fa_frame_parser_push(&node->parser, byte, &request);
    if (result != FA_FRAME_OK) {
        return result;
    }

    return fa_station_node_handle_frame(node, &request, response, response_cap, response_len);
}
