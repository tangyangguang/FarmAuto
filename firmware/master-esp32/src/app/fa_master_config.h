#ifndef FA_MASTER_CONFIG_H
#define FA_MASTER_CONFIG_H

#include "fa_door_service.h"
#include "fa_feed_service.h"

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

FaFeedDeviceConfig fa_master_read_feed_config(void);
FaDoorDeviceConfig fa_master_read_door_config(void);

#endif
