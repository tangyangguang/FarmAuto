#include "fa_device_registry.h"

#include "fa_action_record_store.h"

#include <Esp32Base.h>

extern "C" {
#include "fa_crc.h"
#include "fa_payload.h"
#include "fa_protocol.h"
}

#include <string.h>

namespace {

constexpr uint32_t kMagic = 0x46445231ul;
constexpr uint16_t kSchema = 1u;
constexpr uint16_t kHeaderLen = 32u;
constexpr uint16_t kStationRecordLen = 22u;
constexpr uint16_t kStationSlotLen = 24u;
constexpr uint16_t kDeviceRecordLen = 46u;
constexpr uint16_t kDeviceSlotLen = 48u;
constexpr uint32_t kOnlineSeenPersistIntervalSeconds = 60u;

struct RegistryHeader {
    uint8_t station_capacity = FaDeviceRegistry::kMaxStations;
    uint8_t device_capacity = FaDeviceRegistry::kMaxDevices;
    uint8_t station_count = 0u;
    uint8_t device_count = 0u;
    uint32_t sequence = 0u;
};

RegistryHeader g_header;
FaStationRecord g_stations[FaDeviceRegistry::kMaxStations];
FaDeviceRecord g_devices[FaDeviceRegistry::kMaxDevices];
bool g_ready = false;

uint32_t registryFileSize(void) {
    return kHeaderLen +
           static_cast<uint32_t>(FaDeviceRegistry::kMaxStations) * kStationSlotLen +
           static_cast<uint32_t>(FaDeviceRegistry::kMaxDevices) * kDeviceSlotLen;
}

uint32_t stationOffset(uint8_t index) {
    return kHeaderLen + static_cast<uint32_t>(index) * kStationSlotLen;
}

uint32_t deviceOffset(uint8_t index) {
    return kHeaderLen +
           static_cast<uint32_t>(FaDeviceRegistry::kMaxStations) * kStationSlotLen +
           static_cast<uint32_t>(index) * kDeviceSlotLen;
}

void copyName(char* dest, size_t dest_len, const char* src) {
    if (dest == nullptr || dest_len == 0u) {
        return;
    }
    memset(dest, 0, dest_len);
    if (src != nullptr) {
        strncpy(dest, src, dest_len - 1u);
    }
}

bool writeHeader(const RegistryHeader& header) {
    uint8_t data[kHeaderLen];
    memset(data, 0, sizeof(data));

    FaPayloadWriter writer;
    fa_payload_writer_init(&writer, data, sizeof(data));
    if (fa_payload_write_u32(&writer, kMagic) != FA_PAYLOAD_OK ||
        fa_payload_write_u16(&writer, kSchema) != FA_PAYLOAD_OK ||
        fa_payload_write_u16(&writer, kHeaderLen) != FA_PAYLOAD_OK ||
        fa_payload_write_u16(&writer, kStationSlotLen) != FA_PAYLOAD_OK ||
        fa_payload_write_u16(&writer, kDeviceSlotLen) != FA_PAYLOAD_OK ||
        fa_payload_write_u8(&writer, header.station_capacity) != FA_PAYLOAD_OK ||
        fa_payload_write_u8(&writer, header.device_capacity) != FA_PAYLOAD_OK ||
        fa_payload_write_u8(&writer, header.station_count) != FA_PAYLOAD_OK ||
        fa_payload_write_u8(&writer, header.device_count) != FA_PAYLOAD_OK ||
        fa_payload_write_u32(&writer, header.sequence) != FA_PAYLOAD_OK) {
        return false;
    }

    const uint16_t crc = fa_crc16_modbus(data, 22u);
    data[22] = static_cast<uint8_t>(crc & 0xFFu);
    data[23] = static_cast<uint8_t>(crc >> 8);
    return Esp32BaseFs::writeBytesAt(FaDeviceRegistry::kPath, 0u, data, sizeof(data));
}

bool readHeader(RegistryHeader& header) {
    uint8_t data[kHeaderLen];
    size_t readLen = 0u;
    if (!Esp32BaseFs::readBytesAt(FaDeviceRegistry::kPath, 0u, data, sizeof(data), &readLen) ||
        readLen != sizeof(data)) {
        return false;
    }

    const uint16_t storedCrc = static_cast<uint16_t>(data[22]) | (static_cast<uint16_t>(data[23]) << 8);
    if (storedCrc != fa_crc16_modbus(data, 22u)) {
        return false;
    }

    FaPayloadReader reader;
    uint32_t magic = 0u;
    uint16_t schema = 0u;
    uint16_t headerLen = 0u;
    uint16_t stationSlotLen = 0u;
    uint16_t deviceSlotLen = 0u;
    fa_payload_reader_init(&reader, data, 22u);
    if (fa_payload_read_u32(&reader, &magic) != FA_PAYLOAD_OK ||
        fa_payload_read_u16(&reader, &schema) != FA_PAYLOAD_OK ||
        fa_payload_read_u16(&reader, &headerLen) != FA_PAYLOAD_OK ||
        fa_payload_read_u16(&reader, &stationSlotLen) != FA_PAYLOAD_OK ||
        fa_payload_read_u16(&reader, &deviceSlotLen) != FA_PAYLOAD_OK ||
        fa_payload_read_u8(&reader, &header.station_capacity) != FA_PAYLOAD_OK ||
        fa_payload_read_u8(&reader, &header.device_capacity) != FA_PAYLOAD_OK ||
        fa_payload_read_u8(&reader, &header.station_count) != FA_PAYLOAD_OK ||
        fa_payload_read_u8(&reader, &header.device_count) != FA_PAYLOAD_OK ||
        fa_payload_read_u32(&reader, &header.sequence) != FA_PAYLOAD_OK) {
        return false;
    }

    return magic == kMagic &&
           schema == kSchema &&
           headerLen == kHeaderLen &&
           stationSlotLen == kStationSlotLen &&
           deviceSlotLen == kDeviceSlotLen &&
           header.station_capacity == FaDeviceRegistry::kMaxStations &&
           header.device_capacity == FaDeviceRegistry::kMaxDevices &&
           header.station_count <= header.station_capacity &&
           header.device_count <= header.device_capacity;
}

bool encodeStation(const FaStationRecord& record, uint8_t* out, size_t len) {
    if (out == nullptr || len != kStationSlotLen) {
        return false;
    }
    memset(out, 0, len);
    FaPayloadWriter writer;
    fa_payload_writer_init(&writer, out, kStationRecordLen);
    if (fa_payload_write_u16(&writer, record.station_id) != FA_PAYLOAD_OK ||
        fa_payload_write_u8(&writer, record.bus_address) != FA_PAYLOAD_OK ||
        fa_payload_write_u8(&writer, record.enabled) != FA_PAYLOAD_OK ||
        fa_payload_write_u8(&writer, record.online_state) != FA_PAYLOAD_OK ||
        fa_payload_write_u8(&writer, record.protocol_version) != FA_PAYLOAD_OK ||
        fa_payload_write_u16(&writer, record.firmware_version) != FA_PAYLOAD_OK ||
        fa_payload_write_u32(&writer, record.capability_flags) != FA_PAYLOAD_OK ||
        fa_payload_write_u32(&writer, record.last_seen_at) != FA_PAYLOAD_OK ||
        fa_payload_write_u16(&writer, record.last_error) != FA_PAYLOAD_OK) {
        return false;
    }
    const uint16_t crc = fa_crc16_modbus(out, kStationRecordLen);
    out[kStationRecordLen] = static_cast<uint8_t>(crc & 0xFFu);
    out[kStationRecordLen + 1u] = static_cast<uint8_t>(crc >> 8);
    return true;
}

bool decodeStation(const uint8_t* data, size_t len, FaStationRecord& record) {
    if (data == nullptr || len != kStationSlotLen) {
        return false;
    }
    const uint16_t storedCrc = static_cast<uint16_t>(data[kStationRecordLen]) |
                               (static_cast<uint16_t>(data[kStationRecordLen + 1u]) << 8);
    if (storedCrc != fa_crc16_modbus(data, kStationRecordLen)) {
        return false;
    }

    FaPayloadReader reader;
    memset(&record, 0, sizeof(record));
    fa_payload_reader_init(&reader, data, kStationRecordLen);
    return fa_payload_read_u16(&reader, &record.station_id) == FA_PAYLOAD_OK &&
           fa_payload_read_u8(&reader, &record.bus_address) == FA_PAYLOAD_OK &&
           fa_payload_read_u8(&reader, &record.enabled) == FA_PAYLOAD_OK &&
           fa_payload_read_u8(&reader, &record.online_state) == FA_PAYLOAD_OK &&
           fa_payload_read_u8(&reader, &record.protocol_version) == FA_PAYLOAD_OK &&
           fa_payload_read_u16(&reader, &record.firmware_version) == FA_PAYLOAD_OK &&
           fa_payload_read_u32(&reader, &record.capability_flags) == FA_PAYLOAD_OK &&
           fa_payload_read_u32(&reader, &record.last_seen_at) == FA_PAYLOAD_OK &&
           fa_payload_read_u16(&reader, &record.last_error) == FA_PAYLOAD_OK;
}

bool encodeDevice(const FaDeviceRecord& record, uint8_t* out, size_t len) {
    if (out == nullptr || len != kDeviceSlotLen) {
        return false;
    }
    memset(out, 0, len);
    FaPayloadWriter writer;
    fa_payload_writer_init(&writer, out, kDeviceRecordLen);
    if (fa_payload_write_u16(&writer, record.device_id) != FA_PAYLOAD_OK ||
        fa_payload_write_u8(&writer, record.type) != FA_PAYLOAD_OK ||
        fa_payload_write_u8(&writer, record.enabled) != FA_PAYLOAD_OK ||
        fa_payload_write_u8(&writer, record.archived) != FA_PAYLOAD_OK ||
        fa_payload_write_u8(&writer, 0u) != FA_PAYLOAD_OK ||
        fa_payload_write_u16(&writer, record.display_no) != FA_PAYLOAD_OK ||
        fa_payload_write_u16(&writer, record.sort_order) != FA_PAYLOAD_OK ||
        fa_payload_write_u16(&writer, record.station_id) != FA_PAYLOAD_OK ||
        fa_payload_write_u16(&writer, record.config_version) != FA_PAYLOAD_OK) {
        return false;
    }
    for (uint8_t i = 0u; i < sizeof(record.name); ++i) {
        if (fa_payload_write_u8(&writer, static_cast<uint8_t>(record.name[i])) != FA_PAYLOAD_OK) {
            return false;
        }
    }
    const uint16_t crc = fa_crc16_modbus(out, kDeviceRecordLen);
    out[kDeviceRecordLen] = static_cast<uint8_t>(crc & 0xFFu);
    out[kDeviceRecordLen + 1u] = static_cast<uint8_t>(crc >> 8);
    return true;
}

bool decodeDevice(const uint8_t* data, size_t len, FaDeviceRecord& record) {
    if (data == nullptr || len != kDeviceSlotLen) {
        return false;
    }
    const uint16_t storedCrc = static_cast<uint16_t>(data[kDeviceRecordLen]) |
                               (static_cast<uint16_t>(data[kDeviceRecordLen + 1u]) << 8);
    if (storedCrc != fa_crc16_modbus(data, kDeviceRecordLen)) {
        return false;
    }

    uint8_t reserved = 0u;
    FaPayloadReader reader;
    memset(&record, 0, sizeof(record));
    fa_payload_reader_init(&reader, data, kDeviceRecordLen);
    if (fa_payload_read_u16(&reader, &record.device_id) != FA_PAYLOAD_OK ||
        fa_payload_read_u8(&reader, &record.type) != FA_PAYLOAD_OK ||
        fa_payload_read_u8(&reader, &record.enabled) != FA_PAYLOAD_OK ||
        fa_payload_read_u8(&reader, &record.archived) != FA_PAYLOAD_OK ||
        fa_payload_read_u8(&reader, &reserved) != FA_PAYLOAD_OK ||
        fa_payload_read_u16(&reader, &record.display_no) != FA_PAYLOAD_OK ||
        fa_payload_read_u16(&reader, &record.sort_order) != FA_PAYLOAD_OK ||
        fa_payload_read_u16(&reader, &record.station_id) != FA_PAYLOAD_OK ||
        fa_payload_read_u16(&reader, &record.config_version) != FA_PAYLOAD_OK) {
        return false;
    }
    for (uint8_t i = 0u; i < sizeof(record.name); ++i) {
        uint8_t ch = 0u;
        if (fa_payload_read_u8(&reader, &ch) != FA_PAYLOAD_OK) {
            return false;
        }
        record.name[i] = static_cast<char>(ch);
    }
    record.name[sizeof(record.name) - 1u] = '\0';
    return true;
}

bool writeStationSlot(uint8_t index, const FaStationRecord& record) {
    if (index >= FaDeviceRegistry::kMaxStations) {
        return false;
    }
    uint8_t data[kStationSlotLen];
    return encodeStation(record, data, sizeof(data)) &&
           Esp32BaseFs::writeBytesAt(FaDeviceRegistry::kPath, stationOffset(index), data, sizeof(data));
}

bool writeDeviceSlot(uint8_t index, const FaDeviceRecord& record) {
    if (index >= FaDeviceRegistry::kMaxDevices) {
        return false;
    }
    uint8_t data[kDeviceSlotLen];
    return encodeDevice(record, data, sizeof(data)) &&
           Esp32BaseFs::writeBytesAt(FaDeviceRegistry::kPath, deviceOffset(index), data, sizeof(data));
}

void makeDefaultRecords(void) {
    memset(g_stations, 0, sizeof(g_stations));
    memset(g_devices, 0, sizeof(g_devices));

    g_header.station_capacity = FaDeviceRegistry::kMaxStations;
    g_header.device_capacity = FaDeviceRegistry::kMaxDevices;
    g_header.station_count = 2u;
    g_header.device_count = 2u;
    g_header.sequence = 1u;

    g_stations[0].station_id = 1u;
    g_stations[0].bus_address = 1u;
    g_stations[0].enabled = 1u;

    g_stations[1].station_id = 2u;
    g_stations[1].bus_address = 2u;
    g_stations[1].enabled = 1u;

    g_devices[0].device_id = 1u;
    g_devices[0].type = FA_DEVICE_TYPE_FEEDER;
    g_devices[0].enabled = 1u;
    g_devices[0].display_no = 1u;
    g_devices[0].sort_order = 20u;
    g_devices[0].station_id = 1u;
    g_devices[0].config_version = 1u;
    copyName(g_devices[0].name, sizeof(g_devices[0].name), "Feeder 01");

    g_devices[1].device_id = 2u;
    g_devices[1].type = FA_DEVICE_TYPE_DOOR;
    g_devices[1].enabled = 1u;
    g_devices[1].display_no = 1u;
    g_devices[1].sort_order = 10u;
    g_devices[1].station_id = 2u;
    g_devices[1].config_version = 1u;
    copyName(g_devices[1].name, sizeof(g_devices[1].name), "Door 01");
}

bool flushAll(void) {
    if (!Esp32BaseFs::mkdir(FaActionRecordStore::kDir) && !Esp32BaseFs::exists(FaActionRecordStore::kDir)) {
        return false;
    }
    if (!Esp32BaseFs::exists(FaDeviceRegistry::kPath) &&
        !Esp32BaseFs::createFixedFile(FaDeviceRegistry::kPath, registryFileSize(), 0xFFu)) {
        return false;
    }
    if (Esp32BaseFs::fileSize(FaDeviceRegistry::kPath) != static_cast<int64_t>(registryFileSize())) {
        if (!Esp32BaseFs::createFixedFile(FaDeviceRegistry::kPath, registryFileSize(), 0xFFu)) {
            return false;
        }
    }
    if (!writeHeader(g_header)) {
        return false;
    }
    for (uint8_t i = 0u; i < FaDeviceRegistry::kMaxStations; ++i) {
        if (!writeStationSlot(i, g_stations[i])) {
            return false;
        }
    }
    for (uint8_t i = 0u; i < FaDeviceRegistry::kMaxDevices; ++i) {
        if (!writeDeviceSlot(i, g_devices[i])) {
            return false;
        }
    }
    return true;
}

bool loadAll(void) {
    RegistryHeader header;
    if (!readHeader(header)) {
        return false;
    }

    memset(g_stations, 0, sizeof(g_stations));
    memset(g_devices, 0, sizeof(g_devices));
    for (uint8_t i = 0u; i < FaDeviceRegistry::kMaxStations; ++i) {
        uint8_t data[kStationSlotLen];
        size_t readLen = 0u;
        if (!Esp32BaseFs::readBytesAt(FaDeviceRegistry::kPath, stationOffset(i), data, sizeof(data), &readLen) ||
            readLen != sizeof(data) ||
            !decodeStation(data, sizeof(data), g_stations[i])) {
            return false;
        }
    }
    for (uint8_t i = 0u; i < FaDeviceRegistry::kMaxDevices; ++i) {
        uint8_t data[kDeviceSlotLen];
        size_t readLen = 0u;
        if (!Esp32BaseFs::readBytesAt(FaDeviceRegistry::kPath, deviceOffset(i), data, sizeof(data), &readLen) ||
            readLen != sizeof(data) ||
            !decodeDevice(data, sizeof(data), g_devices[i])) {
            return false;
        }
    }
    g_header = header;
    return true;
}

int findStationIndexById(uint16_t station_id) {
    for (uint8_t i = 0u; i < g_header.station_count; ++i) {
        if (g_stations[i].station_id == station_id) {
            return i;
        }
    }
    return -1;
}

int findStationIndexByAddress(uint8_t bus_address) {
    for (uint8_t i = 0u; i < g_header.station_count; ++i) {
        if (g_stations[i].bus_address == bus_address) {
            return i;
        }
    }
    return -1;
}

int findDeviceIndexById(uint16_t device_id) {
    for (uint8_t i = 0u; i < g_header.device_count; ++i) {
        if (g_devices[i].device_id == device_id) {
            return i;
        }
    }
    return -1;
}

bool persistStation(uint8_t index) {
    ++g_header.sequence;
    return writeStationSlot(index, g_stations[index]) && writeHeader(g_header);
}

bool persistDevice(uint8_t index) {
    ++g_header.sequence;
    return writeDeviceSlot(index, g_devices[index]) && writeHeader(g_header);
}

}  // namespace

bool FaDeviceRegistry::begin() {
    ready_ = false;
    g_ready = false;
    if (!Esp32BaseFs::isReady()) {
        return false;
    }
    const int64_t size = Esp32BaseFs::fileSize(kPath);
    if (!Esp32BaseFs::exists(kPath) || size != static_cast<int64_t>(registryFileSize()) || !loadAll()) {
        makeDefaultRecords();
        if (!flushAll()) {
            return false;
        }
    }
    ready_ = true;
    g_ready = true;
    return true;
}

bool FaDeviceRegistry::isReady() const {
    return ready_ && g_ready;
}

uint32_t FaDeviceRegistry::sequence() const {
    return isReady() ? g_header.sequence : 0u;
}

uint8_t FaDeviceRegistry::stationCount() const {
    return isReady() ? g_header.station_count : 0u;
}

uint8_t FaDeviceRegistry::deviceCount() const {
    return isReady() ? g_header.device_count : 0u;
}

bool FaDeviceRegistry::stationAt(uint8_t index, FaStationRecord& out) const {
    if (!isReady() || index >= g_header.station_count) {
        return false;
    }
    out = g_stations[index];
    return true;
}

bool FaDeviceRegistry::deviceAt(uint8_t index, FaDeviceRecord& out) const {
    if (!isReady() || index >= g_header.device_count) {
        return false;
    }
    out = g_devices[index];
    return true;
}

bool FaDeviceRegistry::deviceById(uint16_t device_id, FaDeviceRecord& out) const {
    if (!isReady()) {
        return false;
    }
    const int index = findDeviceIndexById(device_id);
    if (index < 0) {
        return false;
    }
    out = g_devices[index];
    return true;
}

bool FaDeviceRegistry::deviceByType(uint8_t type, FaDeviceRecord& out) const {
    if (!isReady()) {
        return false;
    }
    for (uint8_t i = 0u; i < g_header.device_count; ++i) {
        if (g_devices[i].type == type && g_devices[i].archived == 0u) {
            out = g_devices[i];
            return true;
        }
    }
    return false;
}

bool FaDeviceRegistry::stationById(uint16_t station_id, FaStationRecord& out) const {
    if (!isReady()) {
        return false;
    }
    const int index = findStationIndexById(station_id);
    if (index < 0) {
        return false;
    }
    out = g_stations[index];
    return true;
}

bool FaDeviceRegistry::stationByAddress(uint8_t bus_address, FaStationRecord& out) const {
    if (!isReady()) {
        return false;
    }
    const int index = findStationIndexByAddress(bus_address);
    if (index < 0) {
        return false;
    }
    out = g_stations[index];
    return true;
}

bool FaDeviceRegistry::setStationEnabled(uint8_t bus_address, bool enabled) {
    if (!isReady() || !fa_address_is_normal(bus_address)) {
        return false;
    }
    const int index = findStationIndexByAddress(bus_address);
    if (index < 0) {
        return false;
    }
    g_stations[index].enabled = enabled ? 1u : 0u;
    return persistStation(static_cast<uint8_t>(index));
}

bool FaDeviceRegistry::setDeviceEnabled(uint16_t device_id, bool enabled) {
    if (!isReady()) {
        return false;
    }
    const int index = findDeviceIndexById(device_id);
    if (index < 0) {
        return false;
    }
    g_devices[index].enabled = enabled ? 1u : 0u;
    return persistDevice(static_cast<uint8_t>(index));
}

bool FaDeviceRegistry::setDeviceName(uint16_t device_id, const char* name) {
    if (!isReady() || name == nullptr || name[0] == '\0') {
        return false;
    }
    const int index = findDeviceIndexById(device_id);
    if (index < 0) {
        return false;
    }
    if (strncmp(g_devices[index].name, name, sizeof(g_devices[index].name)) == 0) {
        return true;
    }
    copyName(g_devices[index].name, sizeof(g_devices[index].name), name);
    return persistDevice(static_cast<uint8_t>(index));
}

bool FaDeviceRegistry::setDeviceStationByAddress(uint16_t device_id, uint8_t bus_address) {
    if (!isReady() || !fa_address_is_normal(bus_address)) {
        return false;
    }
    const int device_index = findDeviceIndexById(device_id);
    const int station_index = findStationIndexByAddress(bus_address);
    if (device_index < 0 || station_index < 0) {
        return false;
    }
    if (g_devices[device_index].station_id == g_stations[station_index].station_id) {
        return true;
    }
    g_devices[device_index].station_id = g_stations[station_index].station_id;
    return persistDevice(static_cast<uint8_t>(device_index));
}

bool FaDeviceRegistry::setDeviceDisplayOrder(uint16_t device_id, uint16_t display_no, uint16_t sort_order) {
    if (!isReady() || display_no == 0u) {
        return false;
    }
    const int index = findDeviceIndexById(device_id);
    if (index < 0) {
        return false;
    }
    if (g_devices[index].display_no == display_no && g_devices[index].sort_order == sort_order) {
        return true;
    }
    g_devices[index].display_no = display_no;
    g_devices[index].sort_order = sort_order;
    return persistDevice(static_cast<uint8_t>(index));
}

bool FaDeviceRegistry::updateStationFromPing(uint8_t bus_address, const FaMasterPingResponse& ping, uint32_t seen_at) {
    if (!isReady() || !fa_address_is_normal(bus_address)) {
        return false;
    }

    int index = findStationIndexByAddress(bus_address);
    if (index < 0) {
        if (g_header.station_count >= g_header.station_capacity) {
            return false;
        }
        index = g_header.station_count;
        ++g_header.station_count;
        memset(&g_stations[index], 0, sizeof(g_stations[index]));
        g_stations[index].station_id = bus_address;
        g_stations[index].bus_address = bus_address;
        g_stations[index].enabled = 1u;
    }

    FaStationRecord& station = g_stations[index];
    station.online_state = FA_STATION_ONLINE_ONLINE;
    station.protocol_version = ping.protocol_version;
    station.firmware_version = ping.firmware_version;
    station.capability_flags = ping.capability_flags;
    station.last_seen_at = seen_at;
    station.last_error = 0u;
    if (ping.effective_bus_address != bus_address || ping.raw_address_input != bus_address) {
        station.online_state = FA_STATION_ONLINE_CONFLICT_SUSPECTED;
    }
    return persistStation(static_cast<uint8_t>(index));
}

bool FaDeviceRegistry::markStationOnline(uint8_t bus_address, uint32_t seen_at) {
    if (!isReady() || !fa_address_is_normal(bus_address)) {
        return false;
    }
    const int index = findStationIndexByAddress(bus_address);
    if (index < 0) {
        return false;
    }
    FaStationRecord& station = g_stations[index];
    const bool should_persist = station.online_state != FA_STATION_ONLINE_ONLINE ||
                                station.last_error != 0u ||
                                seen_at < station.last_seen_at ||
                                seen_at - station.last_seen_at >= kOnlineSeenPersistIntervalSeconds;
    station.online_state = FA_STATION_ONLINE_ONLINE;
    station.last_seen_at = seen_at;
    station.last_error = 0u;
    if (!should_persist) {
        return true;
    }
    return persistStation(static_cast<uint8_t>(index));
}

bool FaDeviceRegistry::markStationOffline(uint8_t bus_address, uint16_t error_code) {
    if (!isReady() || !fa_address_is_normal(bus_address)) {
        return false;
    }
    const int index = findStationIndexByAddress(bus_address);
    if (index < 0) {
        return false;
    }
    FaStationRecord& station = g_stations[index];
    const bool should_persist = station.online_state != FA_STATION_ONLINE_OFFLINE ||
                                station.last_error != error_code;
    station.online_state = FA_STATION_ONLINE_OFFLINE;
    station.last_error = error_code;
    if (!should_persist) {
        return true;
    }
    return persistStation(static_cast<uint8_t>(index));
}

bool FaDeviceRegistry::markStationError(uint8_t bus_address, uint16_t error_code) {
    if (!isReady() || !fa_address_is_normal(bus_address)) {
        return false;
    }
    const int index = findStationIndexByAddress(bus_address);
    if (index < 0) {
        return false;
    }
    FaStationRecord& station = g_stations[index];
    const bool should_persist = station.online_state != FA_STATION_ONLINE_ERROR ||
                                station.last_error != error_code;
    station.online_state = FA_STATION_ONLINE_ERROR;
    station.last_error = error_code;
    if (!should_persist) {
        return true;
    }
    return persistStation(static_cast<uint8_t>(index));
}

bool FaDeviceRegistry::resetDefaults() {
    if (!Esp32BaseFs::isReady()) {
        return false;
    }
    makeDefaultRecords();
    if (!flushAll()) {
        return false;
    }
    ready_ = true;
    g_ready = true;
    return true;
}
