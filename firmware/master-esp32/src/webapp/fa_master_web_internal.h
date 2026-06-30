#ifndef FA_MASTER_WEB_INTERNAL_H
#define FA_MASTER_WEB_INTERNAL_H

#include "fa_master_web.h"

#include <Arduino.h>
#include <Esp32Base.h>

#include "fa_action_record_store.h"

constexpr const char* kNs = "fa_feed";
constexpr const char* kStationAddress = "addr";
constexpr const char* kPulsesPerTurn = "ppt";
constexpr const char* kGramsPerTurnMg = "gpt_mg";
constexpr const char* kDirection = "dir";
constexpr const char* kSpeedPermille = "speed";
constexpr const char* kOverCurrentMa = "oc_ma";
constexpr const char* kMaxRunMs = "max_ms";
constexpr const char* kMaxActionPulses = "max_p";
constexpr uint16_t kSingleFeederDeviceId = 1u;
constexpr uint16_t kSingleDoorDeviceId = 2u;
constexpr uint8_t kRecentRecordLimit = 8u;

constexpr const char* kDoorNs = "fa_door";
constexpr const char* kDoorStationAddress = "addr";
constexpr const char* kDoorPulsesPerTurn = "ppt";
constexpr const char* kDoorTravelPulses = "travel";
constexpr const char* kDoorOpenDirection = "open_dir";
constexpr const char* kDoorCloseDirection = "close_dir";
constexpr const char* kDoorSpeedPermille = "speed";
constexpr const char* kDoorOverCurrentMa = "oc_ma";
constexpr const char* kDoorMaxRunMs = "max_ms";
constexpr const char* kDoorMaxActionPulses = "max_p";

extern FaFeedService* g_feed_service;
extern FaDoorService* g_door_service;
extern FaRs485Master* g_rs485_master;
extern FaRs485Transport* g_transport;
extern FaMasterActionRuntime* g_action_runtime;

uint32_t readUIntParam(const char* name, uint32_t fallback);
const char* statusName(uint8_t status);
void sendNumber(uint32_t value);
const char* frameResultName(FaFrameResult result);
void sendActiveActionPanel(void);
void sendRecentRecordsPanel(void);
void sendFeedTransportError(uint16_t http_code, const char* stage, const char* error_key, const char* error_value);
bool transactAndParseCommon(uint8_t station_address,
                            uint8_t expected_seq,
                            uint8_t expected_cmd,
                            const uint8_t* request,
                            size_t request_len,
                            FaMasterCommonResponse* common,
                            const char* stage);

void sendFeedPage(void);
void sendManualFeedApi(void);
void sendDoorPage(void);
void sendDoorOpenApi(void);
void sendDoorCloseApi(void);
void sendDoorStopApi(void);

#endif
