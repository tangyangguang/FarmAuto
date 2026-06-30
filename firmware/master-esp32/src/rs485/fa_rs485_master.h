#ifndef FA_RS485_MASTER_H
#define FA_RS485_MASTER_H

#include "fa_protocol.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t next_seq;
} FaRs485Master;

typedef struct {
    uint16_t config_version;
    uint16_t flags;
    uint32_t pulses_per_turn;
    uint16_t default_speed_permille;
    uint16_t accel_ms;
    uint16_t decel_ms;
    uint16_t over_current_ma;
    uint16_t over_current_hold_ms;
    uint16_t stall_detect_ms;
    uint16_t stall_min_delta_pulses;
    uint32_t max_run_ms;
    uint32_t max_action_pulses;
} FaMasterMotorConfig;

typedef struct {
    uint32_t action_id;
    uint8_t device_type;
    uint8_t action_type;
    uint8_t target_mode;
    int32_t start_position_pulses;
    int32_t target_pulses;
    int8_t direction;
    uint16_t speed_permille;
    uint32_t max_run_ms;
    uint32_t max_action_pulses;
    uint16_t config_version;
} FaMasterActionRequest;

typedef struct {
    uint8_t status_code;
    uint8_t station_state;
    uint16_t fault_code;
    uint8_t src;
    uint8_t seq;
    uint8_t cmd;
    uint8_t error_flag;
} FaMasterCommonResponse;

typedef struct {
    FaMasterCommonResponse common;
    uint8_t protocol_version;
    uint16_t firmware_version;
    uint8_t effective_bus_address;
    uint8_t raw_address_input;
    uint8_t device_class;
    uint32_t capability_flags;
    uint8_t max_payload_len;
} FaMasterPingResponse;

typedef struct {
    FaMasterCommonResponse common;
    uint8_t motor_state;
    uint32_t active_action_id;
    int32_t position_pulses;
    int32_t target_pulses;
    uint16_t current_ma;
    uint16_t peak_current_ma;
    uint32_t run_ms;
    uint32_t completed_pulses;
    uint8_t last_stop_reason;
} FaMasterStatusResponse;

void fa_rs485_master_init(FaRs485Master *master);
FaFrameResult fa_rs485_master_build_ping(FaRs485Master *master, uint8_t dst, uint8_t *out, size_t out_cap, size_t *out_len, uint8_t *seq_out);
FaFrameResult fa_rs485_master_build_set_motor_config(FaRs485Master *master, uint8_t dst, const FaMasterMotorConfig *config, uint8_t *out, size_t out_cap, size_t *out_len, uint8_t *seq_out);
FaFrameResult fa_rs485_master_build_start_action(FaRs485Master *master, uint8_t dst, const FaMasterActionRequest *request, uint8_t *out, size_t out_cap, size_t *out_len, uint8_t *seq_out);
FaFrameResult fa_rs485_master_build_get_status(FaRs485Master *master, uint8_t dst, uint8_t *out, size_t out_cap, size_t *out_len, uint8_t *seq_out);

uint8_t fa_rs485_master_parse_common(const uint8_t *data, size_t data_len, uint8_t expected_src, uint8_t expected_seq, uint8_t expected_cmd, FaMasterCommonResponse *response);
uint8_t fa_rs485_master_parse_ping(const uint8_t *data, size_t data_len, uint8_t expected_src, uint8_t expected_seq, FaMasterPingResponse *response);
uint8_t fa_rs485_master_parse_status(const uint8_t *data, size_t data_len, uint8_t expected_src, uint8_t expected_seq, FaMasterStatusResponse *response);

#ifdef __cplusplus
}
#endif

#endif
