#ifndef FA_DEVICE_REGISTRY_H
#define FA_DEVICE_REGISTRY_H

#include "fa_rs485_master.h"

#include <stdint.h>

enum FaStationOnlineState : uint8_t {
    FA_STATION_ONLINE_UNKNOWN = 0,
    FA_STATION_ONLINE_ONLINE = 1,
    FA_STATION_ONLINE_OFFLINE = 2,
    FA_STATION_ONLINE_ERROR = 3,
    FA_STATION_ONLINE_CONFLICT_SUSPECTED = 4,
    FA_STATION_ONLINE_RESERVED_ADDRESS = 5
};

struct FaStationRecord {
    uint16_t station_id = 0u;
    uint8_t bus_address = 0u;
    uint8_t enabled = 0u;
    uint8_t online_state = FA_STATION_ONLINE_UNKNOWN;
    uint8_t protocol_version = 0u;
    uint16_t firmware_version = 0u;
    uint32_t capability_flags = 0u;
    uint32_t last_seen_at = 0u;
    uint16_t last_error = 0u;
};

struct FaDeviceRecord {
    uint16_t device_id = 0u;
    uint8_t type = 0u;
    uint8_t enabled = 0u;
    uint8_t archived = 0u;
    uint16_t display_no = 0u;
    uint16_t sort_order = 0u;
    uint16_t station_id = 0u;
    uint16_t config_version = 0u;
    char name[24] = {};
};

class FaDeviceRegistry {
public:
    static constexpr const char* kPath = "/farmauto/device-registry.bin";
    static constexpr uint8_t kMaxStations = 127u;
    static constexpr uint8_t kMaxDevices = 16u;

    bool begin();
    bool isReady() const;
    uint32_t sequence() const;

    uint8_t stationCount() const;
    uint8_t deviceCount() const;
    bool stationAt(uint8_t index, FaStationRecord& out) const;
    bool deviceAt(uint8_t index, FaDeviceRecord& out) const;
    bool deviceById(uint16_t device_id, FaDeviceRecord& out) const;
    bool deviceByType(uint8_t type, FaDeviceRecord& out) const;
    bool stationById(uint16_t station_id, FaStationRecord& out) const;
    bool stationByAddress(uint8_t bus_address, FaStationRecord& out) const;

    bool setDeviceEnabled(uint16_t device_id, bool enabled);
    bool setDeviceStationByAddress(uint16_t device_id, uint8_t bus_address);
    bool updateStationFromPing(uint8_t bus_address, const FaMasterPingResponse& ping, uint32_t seen_at);
    bool markStationOnline(uint8_t bus_address, uint32_t seen_at);
    bool markStationOffline(uint8_t bus_address, uint16_t error_code);
    bool markStationError(uint8_t bus_address, uint16_t error_code);
    bool resetDefaults();

private:
    bool ready_ = false;
};

#endif
