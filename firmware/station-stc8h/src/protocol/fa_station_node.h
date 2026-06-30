#ifndef FA_STATION_NODE_H
#define FA_STATION_NODE_H

#include "fa_action_controller.h"
#include "fa_protocol.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FA_STATION_FIRMWARE_VERSION 0x0001u

typedef struct {
    uint8_t address;
    uint8_t raw_address_input;
    uint32_t now_ms;
    int32_t position_pulses;
    uint16_t current_ma;
    FaActionOutput output;
    FaActionController action;
    FaFrameParser parser;
} FaStationNode;

void fa_station_node_init(FaStationNode *node, uint8_t address);
void fa_station_node_set_address(FaStationNode *node, uint8_t address);
void fa_station_node_tick(FaStationNode *node, uint32_t now_ms, int32_t position_pulses, uint16_t current_ma);
FaFrameResult fa_station_node_handle_frame(FaStationNode *node, const FaFrame *request, uint8_t *response, size_t response_cap, size_t *response_len);
FaFrameResult fa_station_node_push_byte(FaStationNode *node, uint8_t byte, uint8_t *response, size_t response_cap, size_t *response_len);

#ifdef __cplusplus
}
#endif

#endif
