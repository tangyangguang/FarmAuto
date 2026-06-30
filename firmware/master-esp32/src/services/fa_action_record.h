#ifndef FA_ACTION_RECORD_H
#define FA_ACTION_RECORD_H

#include "fa_rs485_master.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    FA_ACTION_RECORD_SOURCE_MANUAL = 1u,
    FA_ACTION_RECORD_SOURCE_SCHEDULE = 2u,
    FA_ACTION_RECORD_SOURCE_MAINTENANCE = 3u
} FaActionRecordSource;

typedef enum {
    FA_ACTION_RECORD_RUNNING = 1u,
    FA_ACTION_RECORD_COMPLETED = 2u,
    FA_ACTION_RECORD_STOPPED = 3u,
    FA_ACTION_RECORD_FAILED = 4u
} FaActionRecordState;

typedef struct {
    uint32_t action_id;
    uint16_t device_id;
    uint8_t bus_address;
    uint8_t device_type;
    uint8_t action_type;
    uint8_t source_type;
    uint16_t source_id;
    uint32_t target_pulses;
    uint8_t amount_mode;
    uint32_t amount_value;
    uint32_t started_at_s;
} FaActionRecordStart;

typedef struct {
    uint32_t action_id;
    uint16_t device_id;
    uint8_t bus_address;
    uint8_t device_type;
    uint8_t action_type;
    uint8_t source_type;
    uint16_t source_id;
    uint32_t target_pulses;
    uint8_t amount_mode;
    uint32_t amount_value;
    uint32_t started_at_s;
    uint32_t ended_at_s;
    uint32_t run_ms;
    uint32_t completed_pulses;
    int32_t final_position_pulses;
    uint16_t current_ma;
    uint16_t peak_current_ma;
    uint8_t stop_reason;
    uint16_t fault_code;
    uint8_t state;
} FaActionRecord;

uint8_t fa_action_record_begin(FaActionRecord *record, const FaActionRecordStart *start);
uint8_t fa_action_record_apply_status(FaActionRecord *record, const FaMasterStatusResponse *status, uint32_t now_s);
uint8_t fa_action_record_is_terminal(const FaActionRecord *record);

#ifdef __cplusplus
}
#endif

#endif
