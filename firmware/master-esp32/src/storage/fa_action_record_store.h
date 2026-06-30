#ifndef FA_ACTION_RECORD_STORE_H
#define FA_ACTION_RECORD_STORE_H

#include "fa_action_record.h"

#include <stdint.h>

class FaActionRecordStore {
public:
    static constexpr const char* kDir = "/farmauto";
    static constexpr const char* kPath = "/farmauto/action-records.bin";
    static constexpr uint16_t kDefaultCapacity = 64;

    static bool begin(uint16_t capacity = kDefaultCapacity);
    static bool append(const FaActionRecord& record);
    static bool readLatest(uint16_t offset, FaActionRecord& record);
    static uint16_t capacity();
    static uint16_t count();
    static uint32_t sequence();
    static bool isReady();
};

#endif
