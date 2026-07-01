#ifndef FA_STATION_POLLER_H
#define FA_STATION_POLLER_H

#include "fa_device_registry.h"
#include "fa_master_action_runtime.h"
#include "fa_rs485_master.h"
#include "fa_rs485_transport.h"

class FaStationPoller {
public:
    static constexpr uint32_t kDefaultPollIntervalMs = 10000u;

    void begin(FaRs485Master* master,
               FaRs485Transport* transport,
               FaDeviceRegistry* registry,
               FaMasterActionRuntime* action_runtime);
    void handle();

private:
    void pollOne();

    FaRs485Master* master_ = nullptr;
    FaRs485Transport* transport_ = nullptr;
    FaDeviceRegistry* registry_ = nullptr;
    FaMasterActionRuntime* action_runtime_ = nullptr;
    uint32_t last_poll_ms_ = 0u;
    uint8_t next_index_ = 0u;
};

#endif
