#pragma once

#include "cmd_sniffer.h"

#ifdef __cplusplus
extern "C" {
#endif

void mqtt_init(void);
void mqtt_handle_packet(sniffer_packet_info_t *packet);

#ifdef __cplusplus
}
#endif
