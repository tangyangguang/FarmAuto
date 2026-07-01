#ifndef FA_MASTER_WEB_INTERNAL_H
#define FA_MASTER_WEB_INTERNAL_H

#include "fa_master_web.h"

#include <Arduino.h>
#include <Esp32Base.h>

#include "fa_action_record_store.h"
#include "fa_master_config.h"

constexpr uint8_t kRecentRecordLimit = 8u;

extern FaFeedService* g_feed_service;
extern FaDoorService* g_door_service;
extern FaDeviceRegistry* g_device_registry;
extern FaRs485Master* g_rs485_master;
extern FaRs485Transport* g_transport;
extern FaMasterActionRuntime* g_action_runtime;
extern FaAutoScheduler* g_auto_scheduler;

struct FaWebDeviceStatus {
    uint16_t device_id = 0u;
    char device_name[FA_ACTION_RECORD_DEVICE_NAME_LEN] = {};
    uint8_t station_address = 0u;
    bool registry_ready = false;
    bool has_device = false;
    bool device_enabled = true;
    bool has_station = false;
    uint8_t station_online_state = FA_STATION_ONLINE_UNKNOWN;
    uint32_t last_seen_at = 0u;
    uint16_t last_error = 0u;
};

uint32_t readUIntParam(const char* name, uint32_t fallback);
const char* statusName(uint8_t status);
void sendNumber(uint32_t value);
void formatDeviceLabel(uint16_t device_id, char* out, size_t len);
void copyActionRecordDeviceName(FaActionRecordStart& start, const FaWebDeviceStatus& status);
const char* stationOnlineStateName(uint8_t state);
bool readDeviceStatus(uint8_t device_type,
                      uint16_t fallback_device_id,
                      uint8_t fallback_station_address,
                      FaWebDeviceStatus& out);
bool deviceStatusBlocksStart(const FaWebDeviceStatus& status);
void formatStationStatusLabel(const FaWebDeviceStatus& status, char* out, size_t len);
void sendDeviceStatusBlockedJson(const FaWebDeviceStatus& status);
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
void sendStopActiveActionApi(void);

void sendFeedPage(void);
void sendManualFeedApi(void);
void sendDoorPage(void);
void sendDoorOpenApi(void);
void sendDoorCloseApi(void);
void sendDoorStopApi(void);
void sendRecordsPage(void);
void sendAutoPage(void);
void sendNotifyPage(void);
void sendAutoFeedPauseApi(void);
void sendAutoFeedResumeApi(void);
void sendAutoDoorPauseApi(void);
void sendAutoDoorResumeApi(void);
void sendBusPage(void);
void sendBusScanApi(void);
void sendDevicesPage(void);
void sendDeviceSetEnabledApi(void);
void sendDeviceNameApi(void);
void sendDeviceDisplayOrderApi(void);
void sendDeviceBindStationApi(void);
void sendStationSetEnabledApi(void);
void sendStationClearFaultApi(void);

#endif
