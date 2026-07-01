#include "fa_device_registry.h"

#include "Esp32Base.h"

#include <stdio.h>
#include <string.h>

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

static void expect_string(const char* actual, const char* expected, const char* message) {
    if (strcmp(actual, expected) != 0) {
        printf("FAIL: %s: got %s expected %s\n", message, actual, expected);
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
    expect_true(registry.setStationEnabled(5u, false), "disable station 5");
    expect_true(registry.stationByAddress(5u, station), "station 5 after disable");
    expect_u32(station.enabled, 0u, "station disabled in memory");
    expect_true(!registry.setStationEnabled(0u, true), "reject reserved station enable address");
    expect_true(!registry.setStationEnabled(99u, true), "reject missing station enable address");
    expect_true(registry.setStationEnabled(5u, true), "enable station 5");
    expect_true(registry.stationByAddress(5u, station), "station 5 after enable");
    expect_u32(station.enabled, 1u, "station enabled in memory");
    expect_true(registry.setDeviceStationByAddress(feeder.device_id, 5u), "bind feeder to station 5");
    expect_true(registry.deviceById(feeder.device_id, feeder), "read rebound feeder");
    expect_u32(feeder.station_id, 5u, "feeder station updated in memory");
    expect_true(!registry.setDeviceStationByAddress(feeder.device_id, 0u), "reject reserved address binding");
    expect_true(!registry.setDeviceStationByAddress(feeder.device_id, 99u), "reject missing station binding");
    expect_true(registry.setDeviceDisplayOrder(feeder.device_id, 3u, 30u), "update feeder display order");
    expect_true(registry.deviceById(feeder.device_id, feeder), "read reordered feeder");
    expect_u32(feeder.display_no, 3u, "feeder display no updated");
    expect_u32(feeder.sort_order, 30u, "feeder sort order updated");
    expect_true(!registry.setDeviceDisplayOrder(feeder.device_id, 0u, 30u), "reject zero display no");
    expect_true(registry.setDeviceName(feeder.device_id, "Feeder A"), "rename feeder");
    expect_true(registry.deviceById(feeder.device_id, feeder), "read renamed feeder");
    expect_string(feeder.name, "Feeder A", "feeder name updated");
    expect_true(!registry.setDeviceName(feeder.device_id, ""), "reject empty device name");

    FaDeviceRegistry reloaded;
    expect_true(reloaded.begin(), "registry reloads existing file");
    expect_true(reloaded.deviceById(1u, feeder), "reloaded feeder exists");
    expect_u32(feeder.enabled, 0u, "feeder disabled persisted after reload");
    expect_u32(feeder.station_id, 5u, "feeder binding persisted after reload");
    expect_u32(feeder.display_no, 3u, "feeder display no persisted after reload");
    expect_u32(feeder.sort_order, 30u, "feeder sort order persisted after reload");
    expect_string(feeder.name, "Feeder A", "feeder name persisted after reload");
    expect_true(reloaded.stationByAddress(5u, station), "reloaded station 5 exists");
    expect_u32(station.enabled, 1u, "station enabled persisted after reload");
    expect_u32(station.firmware_version, 7u, "station firmware persisted after reload");
    expect_u32(station.last_seen_at, 2233u, "station online state persisted after reload");

    if (failures != 0) {
        return 1;
    }
    printf("device registry smoke tests passed\n");
    return 0;
}
