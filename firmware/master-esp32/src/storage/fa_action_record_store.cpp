#include "fa_action_record_store.h"

#include <Esp32Base.h>

extern "C" {
#include "fa_crc.h"
#include "fa_payload.h"
}

#include <string.h>

namespace {

constexpr uint32_t kMagic = 0x46524131ul;
constexpr uint16_t kSchema = 1u;
constexpr uint16_t kHeaderLen = 32u;
constexpr uint16_t kSlotLen = FA_ACTION_RECORD_ENCODED_LEN + 2u;
constexpr uint16_t kMaxCapacity = 128u;

struct StoreHeader {
    uint16_t capacity = 0u;
    uint16_t count = 0u;
    uint16_t nextIndex = 0u;
    uint32_t sequence = 0u;
};

StoreHeader g_header;
bool g_ready = false;

bool writeHeader(const StoreHeader& header) {
    uint8_t data[kHeaderLen];
    memset(data, 0, sizeof(data));

    FaPayloadWriter writer;
    fa_payload_writer_init(&writer, data, sizeof(data));
    if (fa_payload_write_u32(&writer, kMagic) != FA_PAYLOAD_OK ||
        fa_payload_write_u16(&writer, kSchema) != FA_PAYLOAD_OK ||
        fa_payload_write_u16(&writer, kHeaderLen) != FA_PAYLOAD_OK ||
        fa_payload_write_u16(&writer, kSlotLen) != FA_PAYLOAD_OK ||
        fa_payload_write_u16(&writer, header.capacity) != FA_PAYLOAD_OK ||
        fa_payload_write_u16(&writer, header.count) != FA_PAYLOAD_OK ||
        fa_payload_write_u16(&writer, header.nextIndex) != FA_PAYLOAD_OK ||
        fa_payload_write_u32(&writer, header.sequence) != FA_PAYLOAD_OK) {
        return false;
    }

    const uint16_t crc = fa_crc16_modbus(data, 20u);
    data[20] = static_cast<uint8_t>(crc & 0xFFu);
    data[21] = static_cast<uint8_t>(crc >> 8);
    return Esp32BaseFs::writeBytesAt(FaActionRecordStore::kPath, 0u, data, sizeof(data));
}

bool readHeader(StoreHeader& header) {
    uint8_t data[kHeaderLen];
    size_t readLen = 0u;
    if (!Esp32BaseFs::readBytesAt(FaActionRecordStore::kPath, 0u, data, sizeof(data), &readLen) ||
        readLen != sizeof(data)) {
        return false;
    }

    const uint16_t storedCrc = static_cast<uint16_t>(data[20]) | (static_cast<uint16_t>(data[21]) << 8);
    if (storedCrc != fa_crc16_modbus(data, 20u)) {
        return false;
    }

    FaPayloadReader reader;
    uint32_t magic = 0u;
    uint16_t schema = 0u;
    uint16_t headerLen = 0u;
    uint16_t slotLen = 0u;
    fa_payload_reader_init(&reader, data, 20u);
    if (fa_payload_read_u32(&reader, &magic) != FA_PAYLOAD_OK ||
        fa_payload_read_u16(&reader, &schema) != FA_PAYLOAD_OK ||
        fa_payload_read_u16(&reader, &headerLen) != FA_PAYLOAD_OK ||
        fa_payload_read_u16(&reader, &slotLen) != FA_PAYLOAD_OK ||
        fa_payload_read_u16(&reader, &header.capacity) != FA_PAYLOAD_OK ||
        fa_payload_read_u16(&reader, &header.count) != FA_PAYLOAD_OK ||
        fa_payload_read_u16(&reader, &header.nextIndex) != FA_PAYLOAD_OK ||
        fa_payload_read_u32(&reader, &header.sequence) != FA_PAYLOAD_OK) {
        return false;
    }

    if (magic != kMagic || schema != kSchema || headerLen != kHeaderLen || slotLen != kSlotLen ||
        header.capacity == 0u || header.capacity > kMaxCapacity ||
        header.count > header.capacity || header.nextIndex >= header.capacity) {
        return false;
    }
    return true;
}

bool createStore(uint16_t capacity) {
    if (capacity == 0u || capacity > kMaxCapacity) {
        return false;
    }
    if (!Esp32BaseFs::mkdir(FaActionRecordStore::kDir) && !Esp32BaseFs::exists(FaActionRecordStore::kDir)) {
        return false;
    }

    const uint32_t fileSize = kHeaderLen + static_cast<uint32_t>(capacity) * kSlotLen;
    if (!Esp32BaseFs::createFixedFile(FaActionRecordStore::kPath, fileSize, 0xFFu)) {
        return false;
    }

    StoreHeader header;
    header.capacity = capacity;
    header.count = 0u;
    header.nextIndex = 0u;
    header.sequence = 0u;
    if (!writeHeader(header)) {
        return false;
    }
    g_header = header;
    return true;
}

bool readSlot(uint16_t index, FaActionRecord& record) {
    if (!g_ready || index >= g_header.capacity) {
        return false;
    }

    uint8_t data[kSlotLen];
    size_t readLen = 0u;
    const uint32_t offset = kHeaderLen + static_cast<uint32_t>(index) * kSlotLen;
    if (!Esp32BaseFs::readBytesAt(FaActionRecordStore::kPath, offset, data, sizeof(data), &readLen) ||
        readLen != sizeof(data)) {
        return false;
    }

    const uint16_t storedCrc = static_cast<uint16_t>(data[FA_ACTION_RECORD_ENCODED_LEN]) |
                               (static_cast<uint16_t>(data[FA_ACTION_RECORD_ENCODED_LEN + 1u]) << 8);
    if (storedCrc == 0xFFFFu || storedCrc != fa_crc16_modbus(data, FA_ACTION_RECORD_ENCODED_LEN)) {
        return false;
    }

    return fa_action_record_decode(data, FA_ACTION_RECORD_ENCODED_LEN, &record) == FA_STATUS_OK;
}

}  // namespace

bool FaActionRecordStore::begin(uint16_t capacity) {
    g_ready = false;
    if (!Esp32BaseFs::isReady()) {
        return false;
    }

    if (!Esp32BaseFs::exists(kPath)) {
        g_ready = createStore(capacity);
        return g_ready;
    }

    StoreHeader header;
    const int64_t size = Esp32BaseFs::fileSize(kPath);
    if (!readHeader(header) || size != static_cast<int64_t>(kHeaderLen + static_cast<uint32_t>(header.capacity) * kSlotLen)) {
        g_ready = createStore(capacity);
        return g_ready;
    }

    g_header = header;
    g_ready = true;
    return true;
}

bool FaActionRecordStore::append(const FaActionRecord& record) {
    if (!g_ready) {
        return false;
    }

    uint8_t data[kSlotLen];
    memset(data, 0, sizeof(data));
    size_t encodedLen = 0u;
    if (fa_action_record_encode(&record, data, FA_ACTION_RECORD_ENCODED_LEN, &encodedLen) != FA_STATUS_OK ||
        encodedLen != FA_ACTION_RECORD_ENCODED_LEN) {
        return false;
    }

    const uint16_t crc = fa_crc16_modbus(data, FA_ACTION_RECORD_ENCODED_LEN);
    data[FA_ACTION_RECORD_ENCODED_LEN] = static_cast<uint8_t>(crc & 0xFFu);
    data[FA_ACTION_RECORD_ENCODED_LEN + 1u] = static_cast<uint8_t>(crc >> 8);

    const uint32_t offset = kHeaderLen + static_cast<uint32_t>(g_header.nextIndex) * kSlotLen;
    if (!Esp32BaseFs::writeBytesAt(kPath, offset, data, sizeof(data))) {
        return false;
    }

    g_header.nextIndex = static_cast<uint16_t>((g_header.nextIndex + 1u) % g_header.capacity);
    if (g_header.count < g_header.capacity) {
        ++g_header.count;
    }
    ++g_header.sequence;
    return writeHeader(g_header);
}

bool FaActionRecordStore::readLatest(uint16_t offset, FaActionRecord& record) {
    if (!g_ready || offset >= g_header.count) {
        return false;
    }

    const uint16_t latest = g_header.nextIndex == 0u ? static_cast<uint16_t>(g_header.capacity - 1u)
                                                     : static_cast<uint16_t>(g_header.nextIndex - 1u);
    const uint16_t index = static_cast<uint16_t>((latest + g_header.capacity - offset) % g_header.capacity);
    return readSlot(index, record);
}

uint16_t FaActionRecordStore::capacity() {
    return g_ready ? g_header.capacity : 0u;
}

uint16_t FaActionRecordStore::count() {
    return g_ready ? g_header.count : 0u;
}

uint32_t FaActionRecordStore::sequence() {
    return g_ready ? g_header.sequence : 0u;
}

bool FaActionRecordStore::isReady() {
    return g_ready;
}
