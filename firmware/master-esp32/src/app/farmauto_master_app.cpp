#include "farmauto_master_app.h"

#include <Arduino.h>
#include <Esp32Base.h>

#include "fa_action_record_store.h"
#include "fa_master_web.h"
#include "fa_rs485_transport.h"

extern "C" {
#include "fa_feed_service.h"
#include "fa_rs485_master.h"
}

static FaRs485Master g_rs485;
static FaRs485Transport g_transport;
static FaFeedService g_feed;

void farmauto_master_setup(void) {
    Esp32Base::setFirmwareInfo("farmauto-master", "0.1.0");
    fa_master_web_register_config();
    fa_master_web_register_routes(&g_feed, &g_rs485, &g_transport);
    Esp32Base::begin();

    fa_rs485_master_init(&g_rs485);
    const FaRs485TransportConfig rs485_config = FaRs485Transport::readConfig();
    if (!g_transport.begin(rs485_config)) {
        ESP32BASE_LOG_W("farm", "rs485_transport_not_configured");
    }
    fa_feed_service_init(&g_feed, 1u);
    if (!FaActionRecordStore::begin()) {
        ESP32BASE_LOG_W("farm", "action_record_store_unavailable");
    }

    ESP32BASE_LOG_I("farm", "master boot");
}

void farmauto_master_loop(void) {
    Esp32Base::handle();
    delay(10);
}
