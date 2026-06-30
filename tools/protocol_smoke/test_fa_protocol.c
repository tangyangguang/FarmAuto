#include "fa_crc.h"
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

static int test_crc16_modbus_reference(void) {
    const uint8_t data[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
    CHECK(fa_crc16_modbus(data, sizeof(data)) == 0x4B37u);
    return 0;
}

static int test_payload_roundtrip(void) {
    uint8_t buf[FA_MAX_PAYLOAD_LEN];
    FaPayloadWriter writer;
    fa_payload_writer_init(&writer, buf, sizeof(buf));

    CHECK(fa_payload_write_u8(&writer, 0x12u) == FA_PAYLOAD_OK);
    CHECK(fa_payload_write_i8(&writer, -3) == FA_PAYLOAD_OK);
    CHECK(fa_payload_write_u16(&writer, 0x3456u) == FA_PAYLOAD_OK);
    CHECK(fa_payload_write_u32(&writer, 0x789ABCDEul) == FA_PAYLOAD_OK);
    CHECK(fa_payload_write_i32(&writer, -1234567) == FA_PAYLOAD_OK);

    FaPayloadReader reader;
    fa_payload_reader_init(&reader, buf, fa_payload_writer_len(&writer));
    uint8_t u8 = 0u;
    int8_t i8 = 0;
    uint16_t u16 = 0u;
    uint32_t u32 = 0u;
    int32_t i32 = 0;

    CHECK(fa_payload_read_u8(&reader, &u8) == FA_PAYLOAD_OK);
    CHECK(u8 == 0x12u);
    CHECK(fa_payload_read_i8(&reader, &i8) == FA_PAYLOAD_OK);
    CHECK(i8 == -3);
    CHECK(fa_payload_read_u16(&reader, &u16) == FA_PAYLOAD_OK);
    CHECK(u16 == 0x3456u);
    CHECK(fa_payload_read_u32(&reader, &u32) == FA_PAYLOAD_OK);
    CHECK(u32 == 0x789ABCDEul);
    CHECK(fa_payload_read_i32(&reader, &i32) == FA_PAYLOAD_OK);
    CHECK(i32 == -1234567);
    CHECK(fa_payload_reader_remaining(&reader) == 0u);
    CHECK(fa_payload_read_u8(&reader, &u8) == FA_PAYLOAD_ERR_UNDERFLOW);
    return 0;
}

static int test_frame_roundtrip(void) {
    FaFrame frame;
    memset(&frame, 0, sizeof(frame));
    frame.version = FA_PROTOCOL_VERSION;
    frame.flags = 0u;
    frame.dst = 7u;
    frame.src = FA_MASTER_ADDRESS;
    frame.seq = 42u;
    frame.cmd = FA_CMD_START_ACTION;

    FaPayloadWriter writer;
    fa_payload_writer_init(&writer, frame.payload, sizeof(frame.payload));
    CHECK(fa_payload_write_u32(&writer, 0x01020304ul) == FA_PAYLOAD_OK);
    CHECK(fa_payload_write_u8(&writer, FA_DEVICE_TYPE_FEEDER) == FA_PAYLOAD_OK);
    CHECK(fa_payload_write_u8(&writer, FA_ACTION_TYPE_FEED) == FA_PAYLOAD_OK);
    CHECK(fa_payload_write_u8(&writer, FA_TARGET_MODE_RELATIVE_PULSES) == FA_PAYLOAD_OK);
    CHECK(fa_payload_write_i32(&writer, 0) == FA_PAYLOAD_OK);
    CHECK(fa_payload_write_i32(&writer, 4320) == FA_PAYLOAD_OK);
    CHECK(fa_payload_write_i8(&writer, 1) == FA_PAYLOAD_OK);
    CHECK(fa_payload_write_u16(&writer, 800u) == FA_PAYLOAD_OK);
    CHECK(fa_payload_write_u32(&writer, 60000ul) == FA_PAYLOAD_OK);
    CHECK(fa_payload_write_u32(&writer, 432000ul) == FA_PAYLOAD_OK);
    CHECK(fa_payload_write_u16(&writer, 3u) == FA_PAYLOAD_OK);
    frame.len = (uint8_t)fa_payload_writer_len(&writer);

    uint8_t encoded[FA_MAX_FRAME_LEN];
    size_t encoded_len = 0u;
    CHECK(fa_frame_encode(&frame, encoded, sizeof(encoded), &encoded_len) == FA_FRAME_OK);
    CHECK(encoded_len == fa_frame_encoded_len(frame.len));

    FaFrame decoded;
    memset(&decoded, 0, sizeof(decoded));
    CHECK(fa_frame_decode(encoded, encoded_len, &decoded) == FA_FRAME_OK);
    CHECK(decoded.version == frame.version);
    CHECK(decoded.flags == frame.flags);
    CHECK(decoded.dst == frame.dst);
    CHECK(decoded.src == frame.src);
    CHECK(decoded.seq == frame.seq);
    CHECK(decoded.cmd == frame.cmd);
    CHECK(decoded.len == frame.len);
    CHECK(memcmp(decoded.payload, frame.payload, frame.len) == 0);

    encoded[encoded_len - 1u] ^= 0x01u;
    CHECK(fa_frame_decode(encoded, encoded_len, &decoded) == FA_FRAME_ERR_CRC);
    return 0;
}

static int test_frame_validation(void) {
    CHECK(fa_address_is_normal(0u) == 0);
    CHECK(fa_address_is_normal(1u) == 1);
    CHECK(fa_address_is_normal(127u) == 1);
    CHECK(fa_address_is_normal(128u) == 0);

    FaFrame frame;
    memset(&frame, 0, sizeof(frame));
    frame.version = FA_PROTOCOL_VERSION;
    frame.len = FA_MAX_PAYLOAD_LEN + 1u;
    uint8_t out[FA_MAX_FRAME_LEN];
    size_t out_len = 0u;
    CHECK(fa_frame_encode(&frame, out, sizeof(out), &out_len) == FA_FRAME_ERR_PAYLOAD_TOO_LONG);
    return 0;
}

static int test_stream_parser(void) {
    FaFrame frame;
    memset(&frame, 0, sizeof(frame));
    frame.version = FA_PROTOCOL_VERSION;
    frame.dst = 3u;
    frame.src = FA_MASTER_ADDRESS;
    frame.seq = 9u;
    frame.cmd = FA_CMD_PING;
    frame.payload[0] = FA_PROTOCOL_VERSION;
    frame.len = 1u;

    uint8_t encoded[FA_MAX_FRAME_LEN];
    size_t encoded_len = 0u;
    CHECK(fa_frame_encode(&frame, encoded, sizeof(encoded), &encoded_len) == FA_FRAME_OK);

    FaFrameParser parser;
    fa_frame_parser_init(&parser);
    FaFrame parsed;
    memset(&parsed, 0, sizeof(parsed));

    CHECK(fa_frame_parser_push(&parser, 0x00u, &parsed) == FA_FRAME_INCOMPLETE);
    CHECK(fa_frame_parser_push(&parser, 0xFFu, &parsed) == FA_FRAME_INCOMPLETE);

    for (size_t i = 0; i < encoded_len; ++i) {
        FaFrameResult result = fa_frame_parser_push(&parser, encoded[i], &parsed);
        if (i + 1u < encoded_len) {
            CHECK(result == FA_FRAME_INCOMPLETE);
        } else {
            CHECK(result == FA_FRAME_OK);
        }
    }

    CHECK(parsed.cmd == FA_CMD_PING);
    CHECK(parsed.dst == 3u);
    CHECK(parsed.len == 1u);
    CHECK(parsed.payload[0] == FA_PROTOCOL_VERSION);
    return 0;
}

int main(void) {
    CHECK(test_crc16_modbus_reference() == 0);
    CHECK(test_payload_roundtrip() == 0);
    CHECK(test_frame_roundtrip() == 0);
    CHECK(test_frame_validation() == 0);
    CHECK(test_stream_parser() == 0);
    printf("protocol smoke tests passed\n");
    return 0;
}
