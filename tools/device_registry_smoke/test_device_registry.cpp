#include "fa_device_registry.h"

#include "Esp32Base.h"

#include <stdio.h>

static int failures;

static void expect_true(bool value, const char* message) {
    if (!value) {
        printf("FAIL: %s\n", message);
        ++failures;
    }
}

static void expect_u32(uint32_t actual, uint32_t expected, const char* message) {
    if (actual != expected) {
        printf("FAIL: %s: got %lu expected %lu\n",
               message,
               static_cast<unsigned long>(actual),
               static_cast<unsigned long>(expected));
        ++failures;
    }
}

static FaMasterPingResponse make_ping(uint8_t address) {
    FaMasterPingResponse ping = {};
    ping.protocol_version = FA_PROTOCOL_VERSION;
    ping.firmware_version = 7u;
    ping.effective_bus_address = address;
    ping.raw_address_input = address;
    ping.device_class = FA_DEVICE_CLASS_MOTOR_ACTUATOR;
    ping.capability_flags = FA_CAP_MOTOR_BIDIRECTIONAL | FA_CAP_HALL_AB_ENCODER;
    ping.max_payload_len = FA_MAX_PAYLOAD_LEN;
    return ping;
}

int main() {
    Esp32BaseFs::reset();

    FaDeviceRegistry registry;
    expect_true(registry.begin(), "registry begin creates defaults");
    expect_true(registry.isReady(), "registry is ready");
    expect_u32(registry.deviceCount(), 2u, "default device count");
    expect_u32(registry.stationCount(), 2u, "default station count");

    FaDeviceRecord feeder;
    expect_true(registry.deviceByType(FA_DEVICE_TYPE_FEEDER, feeder), "default feeder exists");
    expect_u32(feeder.device_id, 1u, "default feeder id");
    expect_u32(feeder.station_id, 1u, "default feeder station");
    expect_u32(feeder.enabled, 1u, "default feeder enabled");

    expect_true(registry.setDeviceEnabled(feeder.device_id, false), "disable feeder");
    expect_true(registry.deviceById(feeder.device_id, feeder), "read disabled feeder");
    expect_u32(feeder.enabled, 0u, "feeder disabled persisted in memory");

    const FaMasterPingResponse ping = make_ping(5u);
    expect_true(registry.updateStationFromPing(5u, ping, 1234u), "add station from ping");
    expect_u32(registry.stationCount(), 3u, "station count after scan update");

    FaStationRecord station;
    expect_true(registry.stationByAddress(5u, station), "station 5 exists");
    expect_u32(station.station_id, 5u, "station id follows first address");
    expect_u32(station.online_state, FA_STATION_ONLINE_ONLINE, "station online");
    expect_u32(station.firmware_version, 7u, "station firmware captured");
    expect_u32(station.last_seen_at, 1234u, "station seen time captured");
    expect_true(registry.markStationOffline(5u, 0x8006u), "mark station offline");
    expect_true(registry.stationByAddress(5u, station), "station 5 after offline");
    expect_u32(station.online_state, FA_STATION_ONLINE_OFFLINE, "station offline");
    expect_u32(station.last_error, 0x8006u, "offline error captured");
    expect_true(registry.markStationOnline(5u, 2233u), "mark station online");
    expect_true(registry.stationByAddress(5u, station), "station 5 after online");
    expect_u32(station.online_state, FA_STATION_ONLINE_ONLINE, "station back online");
    expect_u32(station.last_seen_at, 2233u, "online seen time captured");
    expect_u32(station.last_error, 0u, "online clears last error");

    FaDeviceRegistry reloaded;
    expect_true(reloaded.begin(), "registry reloads existing file");
    expect_true(reloaded.deviceById(1u, feeder), "reloaded feeder exists");
    expect_u32(feeder.enabled, 0u, "feeder disabled persisted after reload");
    expect_true(reloaded.stationByAddress(5u, station), "reloaded station 5 exists");
    expect_u32(station.firmware_version, 7u, "station firmware persisted after reload");
    expect_u32(station.last_seen_at, 2233u, "station online state persisted after reload");

    if (failures != 0) {
        return 1;
    }
    printf("device registry smoke tests passed\n");
    return 0;
}
