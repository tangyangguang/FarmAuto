#include "fa_action_record.h"

#include "fa_payload.h"

#include <string.h>

uint8_t fa_action_record_begin(FaActionRecord *record, const FaActionRecordStart *start) {
    if (record == NULL || start == NULL || start->action_id == 0u || start->device_id == 0u ||
        !fa_address_is_normal(start->bus_address)) {
        return FA_STATUS_ERR_BAD_PARAM;
    }

    memset(record, 0, sizeof(*record));
    record->action_id = start->action_id;
    record->device_id = start->device_id;
    record->bus_address = start->bus_address;
    record->device_type = start->device_type;
    record->action_type = start->action_type;
    record->source_type = start->source_type;
    record->source_id = start->source_id;
    record->target_pulses = start->target_pulses;
    record->amount_mode = start->amount_mode;
    record->amount_value = start->amount_value;
    record->started_at_s = start->started_at_s;
    record->state = FA_ACTION_RECORD_RUNNING;
    return FA_STATUS_OK;
}

uint8_t fa_action_record_apply_status(FaActionRecord *record, const FaMasterStatusResponse *status, uint32_t now_s) {
    if (record == NULL || status == NULL || record->action_id == 0u ||
        status->active_action_id != record->action_id) {
        return FA_STATUS_ERR_BAD_PARAM;
    }

    record->run_ms = status->run_ms;
    record->completed_pulses = status->completed_pulses;
    record->final_position_pulses = status->position_pulses;
    record->current_ma = status->current_ma;
    record->peak_current_ma = status->peak_current_ma;
    record->stop_reason = status->last_stop_reason;
    record->fault_code = status->common.fault_code;

    if (status->motor_state == FA_MOTOR_COMPLETED) {
        record->state = FA_ACTION_RECORD_COMPLETED;
        record->ended_at_s = now_s;
    } else if (status->motor_state == FA_MOTOR_STOPPED) {
        record->state = FA_ACTION_RECORD_STOPPED;
        record->ended_at_s = now_s;
    } else if (status->motor_state == FA_MOTOR_FAULT || status->common.fault_code != FA_FAULT_NONE) {
        record->state = FA_ACTION_RECORD_FAILED;
        record->ended_at_s = now_s;
    } else {
        record->state = FA_ACTION_RECORD_RUNNING;
    }

    return FA_STATUS_OK;
}

uint8_t fa_action_record_is_terminal(const FaActionRecord *record) {
    if (record == NULL) {
        return 0u;
    }
    return record->state == FA_ACTION_RECORD_COMPLETED ||
           record->state == FA_ACTION_RECORD_STOPPED ||
           record->state == FA_ACTION_RECORD_FAILED;
}

uint8_t fa_action_record_encode(const FaActionRecord *record, uint8_t *out, size_t out_cap, size_t *out_len) {
    FaPayloadWriter writer;

    if (record == NULL || out == NULL) {
        return FA_STATUS_ERR_BAD_PARAM;
    }
    if (out_cap < FA_ACTION_RECORD_ENCODED_LEN) {
        return FA_STATUS_ERR_BAD_LENGTH;
    }

    fa_payload_writer_init(&writer, out, out_cap);
    if (fa_payload_write_u32(&writer, record->action_id) != FA_PAYLOAD_OK ||
        fa_payload_write_u16(&writer, record->device_id) != FA_PAYLOAD_OK ||
        fa_payload_write_u8(&writer, record->bus_address) != FA_PAYLOAD_OK ||
        fa_payload_write_u8(&writer, record->device_type) != FA_PAYLOAD_OK ||
        fa_payload_write_u8(&writer, record->action_type) != FA_PAYLOAD_OK ||
        fa_payload_write_u8(&writer, record->source_type) != FA_PAYLOAD_OK ||
        fa_payload_write_u16(&writer, record->source_id) != FA_PAYLOAD_OK ||
        fa_payload_write_u32(&writer, record->target_pulses) != FA_PAYLOAD_OK ||
        fa_payload_write_u8(&writer, record->amount_mode) != FA_PAYLOAD_OK ||
        fa_payload_write_u32(&writer, record->amount_value) != FA_PAYLOAD_OK ||
        fa_payload_write_u32(&writer, record->started_at_s) != FA_PAYLOAD_OK ||
        fa_payload_write_u32(&writer, record->ended_at_s) != FA_PAYLOAD_OK ||
        fa_payload_write_u32(&writer, record->run_ms) != FA_PAYLOAD_OK ||
        fa_payload_write_u32(&writer, record->completed_pulses) != FA_PAYLOAD_OK ||
        fa_payload_write_i32(&writer, record->final_position_pulses) != FA_PAYLOAD_OK ||
        fa_payload_write_u16(&writer, record->current_ma) != FA_PAYLOAD_OK ||
        fa_payload_write_u16(&writer, record->peak_current_ma) != FA_PAYLOAD_OK ||
        fa_payload_write_u8(&writer, record->stop_reason) != FA_PAYLOAD_OK ||
        fa_payload_write_u16(&writer, record->fault_code) != FA_PAYLOAD_OK ||
        fa_payload_write_u8(&writer, record->state) != FA_PAYLOAD_OK) {
        return FA_STATUS_ERR_BAD_LENGTH;
    }

    if (out_len != NULL) {
        *out_len = fa_payload_writer_len(&writer);
    }
    return FA_STATUS_OK;
}

uint8_t fa_action_record_decode(const uint8_t *data, size_t data_len, FaActionRecord *record) {
    FaPayloadReader reader;

    if (data == NULL || record == NULL) {
        return FA_STATUS_ERR_BAD_PARAM;
    }
    if (data_len < FA_ACTION_RECORD_ENCODED_LEN) {
        return FA_STATUS_ERR_BAD_LENGTH;
    }

    memset(record, 0, sizeof(*record));
    fa_payload_reader_init(&reader, data, data_len);
    if (fa_payload_read_u32(&reader, &record->action_id) != FA_PAYLOAD_OK ||
        fa_payload_read_u16(&reader, &record->device_id) != FA_PAYLOAD_OK ||
        fa_payload_read_u8(&reader, &record->bus_address) != FA_PAYLOAD_OK ||
        fa_payload_read_u8(&reader, &record->device_type) != FA_PAYLOAD_OK ||
        fa_payload_read_u8(&reader, &record->action_type) != FA_PAYLOAD_OK ||
        fa_payload_read_u8(&reader, &record->source_type) != FA_PAYLOAD_OK ||
        fa_payload_read_u16(&reader, &record->source_id) != FA_PAYLOAD_OK ||
        fa_payload_read_u32(&reader, &record->target_pulses) != FA_PAYLOAD_OK ||
        fa_payload_read_u8(&reader, &record->amount_mode) != FA_PAYLOAD_OK ||
        fa_payload_read_u32(&reader, &record->amount_value) != FA_PAYLOAD_OK ||
        fa_payload_read_u32(&reader, &record->started_at_s) != FA_PAYLOAD_OK ||
        fa_payload_read_u32(&reader, &record->ended_at_s) != FA_PAYLOAD_OK ||
        fa_payload_read_u32(&reader, &record->run_ms) != FA_PAYLOAD_OK ||
        fa_payload_read_u32(&reader, &record->completed_pulses) != FA_PAYLOAD_OK ||
        fa_payload_read_i32(&reader, &record->final_position_pulses) != FA_PAYLOAD_OK ||
        fa_payload_read_u16(&reader, &record->current_ma) != FA_PAYLOAD_OK ||
        fa_payload_read_u16(&reader, &record->peak_current_ma) != FA_PAYLOAD_OK ||
        fa_payload_read_u8(&reader, &record->stop_reason) != FA_PAYLOAD_OK ||
        fa_payload_read_u16(&reader, &record->fault_code) != FA_PAYLOAD_OK ||
        fa_payload_read_u8(&reader, &record->state) != FA_PAYLOAD_OK) {
        return FA_STATUS_ERR_BAD_LENGTH;
    }

    return FA_STATUS_OK;
}
