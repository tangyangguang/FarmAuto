#ifndef FAKE_STATION_H
#define FAKE_STATION_H

#include "fa_action_controller.h"
#include "fa_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t address;
    uint32_t now_ms;
    int32_t position_pulses;
    uint16_t current_ma;
    FaActionController action;
    FaActionOutput last_output;
} FakeStation;

void fake_station_init(FakeStation *station, uint8_t address);
void fake_station_step(FakeStation *station, uint32_t delta_ms);
FaFrameResult fake_station_handle(FakeStation *station, const uint8_t *request_data, size_t request_len, uint8_t *response_data, size_t response_cap, size_t *response_len);

#ifdef __cplusplus
}
#endif

#endif
