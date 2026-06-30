#include "fa_action_record.h"

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
