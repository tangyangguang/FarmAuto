#ifndef FA_PAYLOAD_H
#define FA_PAYLOAD_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    FA_PAYLOAD_OK = 0,
    FA_PAYLOAD_ERR_NULL = -1,
    FA_PAYLOAD_ERR_OVERFLOW = -2,
    FA_PAYLOAD_ERR_UNDERFLOW = -3
} FaPayloadResult;

typedef struct {
    uint8_t *data;
    size_t cap;
    size_t pos;
} FaPayloadWriter;

typedef struct {
    const uint8_t *data;
    size_t len;
    size_t pos;
} FaPayloadReader;

void fa_payload_writer_init(FaPayloadWriter *writer, uint8_t *data, size_t cap);
size_t fa_payload_writer_len(const FaPayloadWriter *writer);
FaPayloadResult fa_payload_write_u8(FaPayloadWriter *writer, uint8_t value);
FaPayloadResult fa_payload_write_i8(FaPayloadWriter *writer, int8_t value);
FaPayloadResult fa_payload_write_u16(FaPayloadWriter *writer, uint16_t value);
FaPayloadResult fa_payload_write_u32(FaPayloadWriter *writer, uint32_t value);
FaPayloadResult fa_payload_write_i32(FaPayloadWriter *writer, int32_t value);

void fa_payload_reader_init(FaPayloadReader *reader, const uint8_t *data, size_t len);
size_t fa_payload_reader_remaining(const FaPayloadReader *reader);
FaPayloadResult fa_payload_read_u8(FaPayloadReader *reader, uint8_t *value);
FaPayloadResult fa_payload_read_i8(FaPayloadReader *reader, int8_t *value);
FaPayloadResult fa_payload_read_u16(FaPayloadReader *reader, uint16_t *value);
FaPayloadResult fa_payload_read_u32(FaPayloadReader *reader, uint32_t *value);
FaPayloadResult fa_payload_read_i32(FaPayloadReader *reader, int32_t *value);

#ifdef __cplusplus
}
#endif

#endif
