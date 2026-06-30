#include "fa_protocol.h"

#include "fa_crc.h"

#include <string.h>

enum {
    FA_IDX_SOF0 = 0,
    FA_IDX_SOF1 = 1,
    FA_IDX_VERSION = 2,
    FA_IDX_FLAGS = 3,
    FA_IDX_DST = 4,
    FA_IDX_SRC = 5,
    FA_IDX_SEQ = 6,
    FA_IDX_CMD = 7,
    FA_IDX_LEN = 8,
    FA_IDX_PAYLOAD = 9
};

int fa_address_is_normal(uint8_t address) {
    return address >= FA_ADDRESS_MIN && address <= FA_ADDRESS_MAX;
}

size_t fa_frame_encoded_len(uint8_t payload_len) {
    return (size_t)FA_FRAME_OVERHEAD + (size_t)payload_len;
}

FaFrameResult fa_frame_encode(const FaFrame *frame, uint8_t *out, size_t out_cap, size_t *out_len) {
    if (frame == NULL || out == NULL || out_len == NULL) {
        return FA_FRAME_ERR_NULL;
    }
    if (frame->len > FA_MAX_PAYLOAD_LEN) {
        return FA_FRAME_ERR_PAYLOAD_TOO_LONG;
    }

    const size_t encoded_len = fa_frame_encoded_len(frame->len);
    if (out_cap < encoded_len) {
        return FA_FRAME_ERR_OUTPUT_TOO_SMALL;
    }

    out[FA_IDX_SOF0] = FA_SOF0;
    out[FA_IDX_SOF1] = FA_SOF1;
    out[FA_IDX_VERSION] = frame->version;
    out[FA_IDX_FLAGS] = frame->flags;
    out[FA_IDX_DST] = frame->dst;
    out[FA_IDX_SRC] = frame->src;
    out[FA_IDX_SEQ] = frame->seq;
    out[FA_IDX_CMD] = frame->cmd;
    out[FA_IDX_LEN] = frame->len;
    if (frame->len > 0u) {
        memcpy(&out[FA_IDX_PAYLOAD], frame->payload, frame->len);
    }

    const size_t crc_input_len = 7u + (size_t)frame->len;
    const uint16_t crc = fa_crc16_modbus(&out[FA_IDX_VERSION], crc_input_len);
    const size_t crc_index = FA_IDX_PAYLOAD + (size_t)frame->len;
    out[crc_index] = (uint8_t)(crc & 0xFFu);
    out[crc_index + 1u] = (uint8_t)(crc >> 8);
    *out_len = encoded_len;

    return FA_FRAME_OK;
}

FaFrameResult fa_frame_decode(const uint8_t *data, size_t data_len, FaFrame *frame) {
    if (data == NULL || frame == NULL) {
        return FA_FRAME_ERR_NULL;
    }
    if (data_len < FA_FRAME_OVERHEAD) {
        return FA_FRAME_ERR_TOO_SHORT;
    }
    if (data[FA_IDX_SOF0] != FA_SOF0 || data[FA_IDX_SOF1] != FA_SOF1) {
        return FA_FRAME_ERR_BAD_SOF;
    }
    if (data[FA_IDX_VERSION] != FA_PROTOCOL_VERSION) {
        return FA_FRAME_ERR_UNSUPPORTED_VERSION;
    }

    const uint8_t payload_len = data[FA_IDX_LEN];
    if (payload_len > FA_MAX_PAYLOAD_LEN) {
        return FA_FRAME_ERR_PAYLOAD_TOO_LONG;
    }

    const size_t expected_len = fa_frame_encoded_len(payload_len);
    if (data_len != expected_len) {
        return FA_FRAME_ERR_LENGTH_MISMATCH;
    }

    const size_t crc_index = FA_IDX_PAYLOAD + (size_t)payload_len;
    const uint16_t expected_crc = (uint16_t)data[crc_index] | ((uint16_t)data[crc_index + 1u] << 8);
    const uint16_t actual_crc = fa_crc16_modbus(&data[FA_IDX_VERSION], 7u + (size_t)payload_len);
    if (actual_crc != expected_crc) {
        return FA_FRAME_ERR_CRC;
    }

    frame->version = data[FA_IDX_VERSION];
    frame->flags = data[FA_IDX_FLAGS];
    frame->dst = data[FA_IDX_DST];
    frame->src = data[FA_IDX_SRC];
    frame->seq = data[FA_IDX_SEQ];
    frame->cmd = data[FA_IDX_CMD];
    frame->len = payload_len;
    if (payload_len > 0u) {
        memcpy(frame->payload, &data[FA_IDX_PAYLOAD], payload_len);
    }

    return FA_FRAME_OK;
}

void fa_frame_parser_init(FaFrameParser *parser) {
    if (parser == NULL) {
        return;
    }
    parser->len = 0u;
    parser->expected_len = 0u;
}

FaFrameResult fa_frame_parser_push(FaFrameParser *parser, uint8_t byte, FaFrame *frame) {
    if (parser == NULL || frame == NULL) {
        return FA_FRAME_ERR_NULL;
    }

    if (parser->len == 0u) {
        if (byte != FA_SOF0) {
            return FA_FRAME_INCOMPLETE;
        }
        parser->buffer[parser->len++] = byte;
        return FA_FRAME_INCOMPLETE;
    }

    if (parser->len == 1u) {
        if (byte == FA_SOF0) {
            parser->buffer[0] = byte;
            parser->len = 1u;
            parser->expected_len = 0u;
            return FA_FRAME_INCOMPLETE;
        }
        if (byte != FA_SOF1) {
            fa_frame_parser_init(parser);
            return FA_FRAME_INCOMPLETE;
        }
        parser->buffer[parser->len++] = byte;
        return FA_FRAME_INCOMPLETE;
    }

    if (parser->len >= FA_MAX_FRAME_LEN) {
        fa_frame_parser_init(parser);
        return FA_FRAME_ERR_LENGTH_MISMATCH;
    }

    parser->buffer[parser->len++] = byte;

    if (parser->len == (FA_IDX_LEN + 1u)) {
        const uint8_t payload_len = parser->buffer[FA_IDX_LEN];
        if (payload_len > FA_MAX_PAYLOAD_LEN) {
            fa_frame_parser_init(parser);
            return FA_FRAME_ERR_PAYLOAD_TOO_LONG;
        }
        parser->expected_len = fa_frame_encoded_len(payload_len);
    }

    if (parser->expected_len == 0u || parser->len < parser->expected_len) {
        return FA_FRAME_INCOMPLETE;
    }

    if (parser->len > parser->expected_len) {
        fa_frame_parser_init(parser);
        return FA_FRAME_ERR_LENGTH_MISMATCH;
    }

    const FaFrameResult result = fa_frame_decode(parser->buffer, parser->len, frame);
    fa_frame_parser_init(parser);
    return result;
}
