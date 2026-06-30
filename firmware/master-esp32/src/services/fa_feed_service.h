#ifndef FA_FEED_SERVICE_H
#define FA_FEED_SERVICE_H

#include "fa_rs485_master.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    FA_FEED_AMOUNT_TURNS_X1000 = 1u,
    FA_FEED_AMOUNT_MG = 2u
} FaFeedAmountMode;

typedef struct {
    uint8_t station_address;
    uint16_t config_version;
    uint32_t pulses_per_turn;
    uint32_t grams_per_turn_mg;
    int8_t feed_direction;
    uint16_t speed_permille;
    uint16_t accel_ms;
    uint16_t decel_ms;
    uint16_t over_current_ma;
    uint16_t over_current_hold_ms;
    uint16_t stall_detect_ms;
    uint16_t stall_min_delta_pulses;
    uint32_t max_run_ms;
    uint32_t max_action_pulses;
} FaFeedDeviceConfig;

typedef struct {
    uint32_t next_action_id;
} FaFeedService;

typedef struct {
    uint32_t action_id;
    uint32_t target_pulses;
    uint8_t completed;
    uint8_t failed;
    uint8_t stop_reason;
    uint16_t fault_code;
    uint32_t completed_pulses;
} FaFeedResult;

void fa_feed_service_init(FaFeedService *service, uint32_t first_action_id);
uint8_t fa_feed_calculate_target_pulses(const FaFeedDeviceConfig *config, uint8_t mode, uint32_t amount, uint32_t *target_pulses);
uint8_t fa_feed_make_motor_config(const FaFeedDeviceConfig *config, FaMasterMotorConfig *motor_config);
uint8_t fa_feed_make_manual_action(FaFeedService *service, const FaFeedDeviceConfig *config, uint8_t mode, uint32_t amount, FaMasterActionRequest *action, FaFeedResult *result);
uint8_t fa_feed_result_from_status(const FaMasterStatusResponse *status, FaFeedResult *result);

#ifdef __cplusplus
}
#endif

#endif
