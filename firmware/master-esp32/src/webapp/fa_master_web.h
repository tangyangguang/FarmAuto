#ifndef FA_MASTER_WEB_H
#define FA_MASTER_WEB_H

#include "fa_feed_service.h"

void fa_master_web_register_config(void);
void fa_master_web_register_routes(FaFeedService *feed_service);

#endif
