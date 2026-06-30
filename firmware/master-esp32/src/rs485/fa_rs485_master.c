#include "fa_rs485_master.h"

#include "fa_payload.h"

#include <string.h>

static uint8_t fa_master_next_seq(FaRs485Master *master) {
    uint8_t seq = master->next_seq;
    master->next_seq = (uint8_t)(master->next_seq + 1u);
    if (master->next_seq == 0u) {
        master->next_seq = 1u;
    }
    return seq;
}

static FaFrameResult fa_master_encode(FaRs485Master *master, uint8_t dst, uint8_t cmd, uint8_t *payload, uint8_t payload_len, uint8_t *out, size_t out_cap, size_t *out_len, uint8_t *seq_out) {
    if (master == NULL || out == NULL || out_len == NULL) {
        return FA_FRAME_ERR_NULL;
    }
    if (!fa_address_is_normal(dst)) {
        return FA_FRAME_ERR_BAD_SOF;
    }

    FaFrame frame;
    memset(&frame, 0, sizeof(frame));
    frame.version = FA_PROTOCOL_VERSION;
    frame.dst = dst;
    frame.src = FA_MASTER_ADDRESS;
    frame.seq = fa_master_next_seq(master);
    frame.cmd = cmd;
    frame.len = payload_len;
    if (payload_len > 0u && payload != NULL) {
        memcpy(frame.payload, payload, payload_len);
    }

    if (seq_out != NULL) {
        *seq_out = frame.seq;
    }
    return fa_frame_encode(&frame, out, out_cap, out_len);
}

static uint8_t parse_common_frame(const uint8_t *data, size_t data_len, uint8_t expected_src, uint8_t expected_seq, uint8_t expected_cmd, FaFrame *frame, FaPayloadReader *reader, FaMasterCommonResponse *response) {
    if (frame == NULL || reader == NULL || response == NULL) {
        return FA_STATUS_ERR_INTERNAL;
    }
    if (fa_frame_decode(data, data_len, frame) != FA_FRAME_OK) {
        return FA_STATUS_ERR_BAD_PARAM;
    }
    if ((frame->flags & FA_FRAME_FLAG_RESPONSE) == 0u ||
        frame->dst != FA_MASTER_ADDRESS ||
        frame->src != expected_src ||
        frame->seq != expected_seq ||
        frame->cmd != expected_cmd) {
        return FA_STATUS_ERR_BAD_PARAM;
    }

    fa_payload_reader_init(reader, frame->payload, frame->len);
    memset(response, 0, sizeof(*response));
    if (fa_payload_read_u8(reader, &response->status_code) != FA_PAYLOAD_OK ||
        fa_payload_read_u8(reader, &response->station_state) != FA_PAYLOAD_OK ||
        fa_payload_read_u16(reader, &response->fault_code) != FA_PAYLOAD_OK) {
        return FA_STATUS_ERR_BAD_LENGTH;
    }

    response->src = frame->src;
    response->seq = frame->seq;
    response->cmd = frame->cmd;
    response->error_flag = (frame->flags & FA_FRAME_FLAG_ERROR) != 0u ? 1u : 0u;
    return response->status_code;
}

void fa_rs485_master_init(FaRs485Master *master) {
    if (master == NULL) {
        return;
    }
    master->next_seq = 1u;
}

FaFrameResult fa_rs485_master_build_ping(FaRs485Master *master, uint8_t dst, uint8_t *out, size_t out_cap, size_t *out_len, uint8_t *seq_out) {
    uint8_t payload[1] = {FA_PROTOCOL_VERSION};
    return fa_master_encode(master, dst, FA_CMD_PING, payload, sizeof(payload), out, out_cap, out_len, seq_out);
}

FaFrameResult fa_rs485_master_build_set_motor_config(FaRs485Master *master, uint8_t dst, const FaMasterMotorConfig *config, uint8_t *out, size_t out_cap, size_t *out_len, uint8_t *seq_out) {
    if (config == NULL) {
        return FA_FRAME_ERR_NULL;
    }

    uint8_t payload[FA_MAX_PAYLOAD_LEN];
    FaPayloadWriter writer;
    fa_payload_writer_init(&writer, payload, sizeof(payload));
    if (fa_payload_write_u16(&writer, config->config_version) != FA_PAYLOAD_OK ||
        fa_payload_write_u16(&writer, config->flags) != FA_PAYLOAD_OK ||
        fa_payload_write_u32(&writer, config->pulses_per_turn) != FA_PAYLOAD_OK ||
        fa_payload_write_u16(&writer, config->default_speed_permille) != FA_PAYLOAD_OK ||
        fa_payload_write_u16(&writer, config->accel_ms) != FA_PAYLOAD_OK ||
        fa_payload_write_u16(&writer, config->decel_ms) != FA_PAYLOAD_OK ||
        fa_payload_write_u16(&writer, config->over_current_ma) != FA_PAYLOAD_OK ||
        fa_payload_write_u16(&writer, config->over_current_hold_ms) != FA_PAYLOAD_OK ||
        fa_payload_write_u16(&writer, config->stall_detect_ms) != FA_PAYLOAD_OK ||
        fa_payload_write_u16(&writer, config->stall_min_delta_pulses) != FA_PAYLOAD_OK ||
        fa_payload_write_u32(&writer, config->max_run_ms) != FA_PAYLOAD_OK ||
        fa_payload_write_u32(&writer, config->max_action_pulses) != FA_PAYLOAD_OK) {
        return FA_FRAME_ERR_PAYLOAD_TOO_LONG;
    }

    return fa_master_encode(master, dst, FA_CMD_SET_MOTOR_CONFIG, payload, (uint8_t)fa_payload_writer_len(&writer), out, out_cap, out_len, seq_out);
}

FaFrameResult fa_rs485_master_build_start_action(FaRs485Master *master, uint8_t dst, const FaMasterActionRequest *request, uint8_t *out, size_t out_cap, size_t *out_len, uint8_t *seq_out) {
    if (request == NULL) {
        return FA_FRAME_ERR_NULL;
    }

    uint8_t payload[FA_MAX_PAYLOAD_LEN];
    FaPayloadWriter writer;
    fa_payload_writer_init(&writer, payload, sizeof(payload));
    if (fa_payload_write_u32(&writer, request->action_id) != FA_PAYLOAD_OK ||
        fa_payload_write_u8(&writer, request->device_type) != FA_PAYLOAD_OK ||
        fa_payload_write_u8(&writer, request->action_type) != FA_PAYLOAD_OK ||
        fa_payload_write_u8(&writer, request->target_mode) != FA_PAYLOAD_OK ||
        fa_payload_write_i32(&writer, request->start_position_pulses) != FA_PAYLOAD_OK ||
        fa_payload_write_i32(&writer, request->target_pulses) != FA_PAYLOAD_OK ||
        fa_payload_write_i8(&writer, request->direction) != FA_PAYLOAD_OK ||
        fa_payload_write_u16(&writer, request->speed_permille) != FA_PAYLOAD_OK ||
        fa_payload_write_u32(&writer, request->max_run_ms) != FA_PAYLOAD_OK ||
        fa_payload_write_u32(&writer, request->max_action_pulses) != FA_PAYLOAD_OK ||
        fa_payload_write_u16(&writer, request->config_version) != FA_PAYLOAD_OK) {
        return FA_FRAME_ERR_PAYLOAD_TOO_LONG;
    }

    return fa_master_encode(master, dst, FA_CMD_START_ACTION, payload, (uint8_t)fa_payload_writer_len(&writer), out, out_cap, out_len, seq_out);
}

FaFrameResult fa_rs485_master_build_stop_action(FaRs485Master *master, uint8_t dst, uint8_t *out, size_t out_cap, size_t *out_len, uint8_t *seq_out) {
    return fa_master_encode(master, dst, FA_CMD_STOP_ACTION, NULL, 0u, out, out_cap, out_len, seq_out);
}

FaFrameResult fa_rs485_master_build_get_status(FaRs485Master *master, uint8_t dst, uint8_t *out, size_t out_cap, size_t *out_len, uint8_t *seq_out) {
    return fa_master_encode(master, dst, FA_CMD_GET_STATUS, NULL, 0u, out, out_cap, out_len, seq_out);
}

uint8_t fa_rs485_master_parse_common(const uint8_t *data, size_t data_len, uint8_t expected_src, uint8_t expected_seq, uint8_t expected_cmd, FaMasterCommonResponse *response) {
    FaFrame frame;
    FaPayloadReader reader;
    return parse_common_frame(data, data_len, expected_src, expected_seq, expected_cmd, &frame, &reader, response);
}

uint8_t fa_rs485_master_parse_ping(const uint8_t *data, size_t data_len, uint8_t expected_src, uint8_t expected_seq, FaMasterPingResponse *response) {
    if (response == NULL) {
        return FA_STATUS_ERR_INTERNAL;
    }

    FaFrame frame;
    FaPayloadReader reader;
    uint8_t status = parse_common_frame(data, data_len, expected_src, expected_seq, FA_CMD_PING, &frame, &reader, &response->common);
    if (status != FA_STATUS_OK) {
        return status;
    }

    if (fa_payload_read_u8(&reader, &response->protocol_version) != FA_PAYLOAD_OK ||
        fa_payload_read_u16(&reader, &response->firmware_version) != FA_PAYLOAD_OK ||
        fa_payload_read_u8(&reader, &response->effective_bus_address) != FA_PAYLOAD_OK ||
        fa_payload_read_u8(&reader, &response->raw_address_input) != FA_PAYLOAD_OK ||
        fa_payload_read_u8(&reader, &response->device_class) != FA_PAYLOAD_OK ||
        fa_payload_read_u32(&reader, &response->capability_flags) != FA_PAYLOAD_OK ||
        fa_payload_read_u8(&reader, &response->max_payload_len) != FA_PAYLOAD_OK) {
        return FA_STATUS_ERR_BAD_LENGTH;
    }
    return FA_STATUS_OK;
}

uint8_t fa_rs485_master_parse_status(const uint8_t *data, size_t data_len, uint8_t expected_src, uint8_t expected_seq, FaMasterStatusResponse *response) {
    if (response == NULL) {
        return FA_STATUS_ERR_INTERNAL;
    }

    FaFrame frame;
    FaPayloadReader reader;
    uint8_t status = parse_common_frame(data, data_len, expected_src, expected_seq, FA_CMD_GET_STATUS, &frame, &reader, &response->common);
    if (status != FA_STATUS_OK) {
        return status;
    }

    if (fa_payload_read_u8(&reader, &response->motor_state) != FA_PAYLOAD_OK ||
        fa_payload_read_u32(&reader, &response->active_action_id) != FA_PAYLOAD_OK ||
        fa_payload_read_i32(&reader, &response->position_pulses) != FA_PAYLOAD_OK ||
        fa_payload_read_i32(&reader, &response->target_pulses) != FA_PAYLOAD_OK ||
        fa_payload_read_u16(&reader, &response->current_ma) != FA_PAYLOAD_OK ||
        fa_payload_read_u16(&reader, &response->peak_current_ma) != FA_PAYLOAD_OK ||
        fa_payload_read_u32(&reader, &response->run_ms) != FA_PAYLOAD_OK ||
        fa_payload_read_u32(&reader, &response->completed_pulses) != FA_PAYLOAD_OK ||
        fa_payload_read_u8(&reader, &response->last_stop_reason) != FA_PAYLOAD_OK) {
        return FA_STATUS_ERR_BAD_LENGTH;
    }
    return FA_STATUS_OK;
}
