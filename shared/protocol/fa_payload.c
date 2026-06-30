#include "fa_payload.h"

static FaPayloadResult fa_payload_check_write(FaPayloadWriter *writer, size_t count) {
    if (writer == NULL || writer->data == NULL) {
        return FA_PAYLOAD_ERR_NULL;
    }
    if (writer->pos + count > writer->cap) {
        return FA_PAYLOAD_ERR_OVERFLOW;
    }
    return FA_PAYLOAD_OK;
}

static FaPayloadResult fa_payload_check_read(FaPayloadReader *reader, size_t count) {
    if (reader == NULL || reader->data == NULL) {
        return FA_PAYLOAD_ERR_NULL;
    }
    if (reader->pos + count > reader->len) {
        return FA_PAYLOAD_ERR_UNDERFLOW;
    }
    return FA_PAYLOAD_OK;
}

void fa_payload_writer_init(FaPayloadWriter *writer, uint8_t *data, size_t cap) {
    if (writer == NULL) {
        return;
    }
    writer->data = data;
    writer->cap = cap;
    writer->pos = 0u;
}

size_t fa_payload_writer_len(const FaPayloadWriter *writer) {
    return writer == NULL ? 0u : writer->pos;
}

FaPayloadResult fa_payload_write_u8(FaPayloadWriter *writer, uint8_t value) {
    FaPayloadResult result = fa_payload_check_write(writer, 1u);
    if (result != FA_PAYLOAD_OK) {
        return result;
    }
    writer->data[writer->pos++] = value;
    return FA_PAYLOAD_OK;
}

FaPayloadResult fa_payload_write_i8(FaPayloadWriter *writer, int8_t value) {
    return fa_payload_write_u8(writer, (uint8_t)value);
}

FaPayloadResult fa_payload_write_u16(FaPayloadWriter *writer, uint16_t value) {
    FaPayloadResult result = fa_payload_check_write(writer, 2u);
    if (result != FA_PAYLOAD_OK) {
        return result;
    }
    writer->data[writer->pos++] = (uint8_t)(value & 0xFFu);
    writer->data[writer->pos++] = (uint8_t)(value >> 8);
    return FA_PAYLOAD_OK;
}

FaPayloadResult fa_payload_write_u32(FaPayloadWriter *writer, uint32_t value) {
    FaPayloadResult result = fa_payload_check_write(writer, 4u);
    if (result != FA_PAYLOAD_OK) {
        return result;
    }
    writer->data[writer->pos++] = (uint8_t)(value & 0xFFu);
    writer->data[writer->pos++] = (uint8_t)((value >> 8) & 0xFFu);
    writer->data[writer->pos++] = (uint8_t)((value >> 16) & 0xFFu);
    writer->data[writer->pos++] = (uint8_t)((value >> 24) & 0xFFu);
    return FA_PAYLOAD_OK;
}

FaPayloadResult fa_payload_write_i32(FaPayloadWriter *writer, int32_t value) {
    return fa_payload_write_u32(writer, (uint32_t)value);
}

void fa_payload_reader_init(FaPayloadReader *reader, const uint8_t *data, size_t len) {
    if (reader == NULL) {
        return;
    }
    reader->data = data;
    reader->len = len;
    reader->pos = 0u;
}

size_t fa_payload_reader_remaining(const FaPayloadReader *reader) {
    if (reader == NULL || reader->pos > reader->len) {
        return 0u;
    }
    return reader->len - reader->pos;
}

FaPayloadResult fa_payload_read_u8(FaPayloadReader *reader, uint8_t *value) {
    if (value == NULL) {
        return FA_PAYLOAD_ERR_NULL;
    }
    FaPayloadResult result = fa_payload_check_read(reader, 1u);
    if (result != FA_PAYLOAD_OK) {
        return result;
    }
    *value = reader->data[reader->pos++];
    return FA_PAYLOAD_OK;
}

FaPayloadResult fa_payload_read_i8(FaPayloadReader *reader, int8_t *value) {
    if (value == NULL) {
        return FA_PAYLOAD_ERR_NULL;
    }
    uint8_t raw = 0u;
    FaPayloadResult result = fa_payload_read_u8(reader, &raw);
    if (result != FA_PAYLOAD_OK) {
        return result;
    }
    *value = (int8_t)raw;
    return FA_PAYLOAD_OK;
}

FaPayloadResult fa_payload_read_u16(FaPayloadReader *reader, uint16_t *value) {
    if (value == NULL) {
        return FA_PAYLOAD_ERR_NULL;
    }
    FaPayloadResult result = fa_payload_check_read(reader, 2u);
    if (result != FA_PAYLOAD_OK) {
        return result;
    }
    *value = (uint16_t)reader->data[reader->pos] | ((uint16_t)reader->data[reader->pos + 1u] << 8);
    reader->pos += 2u;
    return FA_PAYLOAD_OK;
}

FaPayloadResult fa_payload_read_u32(FaPayloadReader *reader, uint32_t *value) {
    if (value == NULL) {
        return FA_PAYLOAD_ERR_NULL;
    }
    FaPayloadResult result = fa_payload_check_read(reader, 4u);
    if (result != FA_PAYLOAD_OK) {
        return result;
    }
    *value = (uint32_t)reader->data[reader->pos] |
             ((uint32_t)reader->data[reader->pos + 1u] << 8) |
             ((uint32_t)reader->data[reader->pos + 2u] << 16) |
             ((uint32_t)reader->data[reader->pos + 3u] << 24);
    reader->pos += 4u;
    return FA_PAYLOAD_OK;
}

FaPayloadResult fa_payload_read_i32(FaPayloadReader *reader, int32_t *value) {
    if (value == NULL) {
        return FA_PAYLOAD_ERR_NULL;
    }
    uint32_t raw = 0u;
    FaPayloadResult result = fa_payload_read_u32(reader, &raw);
    if (result != FA_PAYLOAD_OK) {
        return result;
    }
    *value = (int32_t)raw;
    return FA_PAYLOAD_OK;
}
