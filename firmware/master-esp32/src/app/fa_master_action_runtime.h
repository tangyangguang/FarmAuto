#ifndef FA_MASTER_ACTION_RUNTIME_H
#define FA_MASTER_ACTION_RUNTIME_H

#include "fa_action_record.h"
#include "fa_rs485_master.h"
#include "fa_rs485_transport.h"

class FaMasterActionRuntime {
public:
    static constexpr uint32_t kDefaultPollIntervalMs = 250u;
    static constexpr uint8_t kMaxPollFailures = 5u;

    void begin(FaRs485Master* master, FaRs485Transport* transport);
    void handle();

    bool trackStartedAction(const FaActionRecordStart& start);
    bool isBusy() const;
    const FaActionRecord* activeRecord() const;
    const char* lastError() const;

    static uint32_t nowSeconds();

private:
    void pollStatus(uint32_t now_ms);
    void finishActiveRecord();
    void failActiveRecord(uint16_t fault_code, uint8_t stop_reason);

    FaRs485Master* master_ = nullptr;
    FaRs485Transport* transport_ = nullptr;
    FaActionRecord active_ = {};
    bool busy_ = false;
    uint32_t last_poll_ms_ = 0u;
    uint8_t poll_failures_ = 0u;
    const char* last_error_ = "none";
};

#endif
