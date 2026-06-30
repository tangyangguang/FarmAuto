#ifndef FA_MASTER_WEB_H
#define FA_MASTER_WEB_H

#include "fa_master_action_runtime.h"
#include "fa_feed_service.h"
#include "fa_rs485_transport.h"

void fa_master_web_register_config(void);
void fa_master_web_register_routes(FaFeedService *feed_service,
                                   FaRs485Master *rs485_master,
                                   FaRs485Transport *transport,
                                   FaMasterActionRuntime *action_runtime);

#endif
