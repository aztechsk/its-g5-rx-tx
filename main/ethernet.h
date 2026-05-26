#pragma once

#include <stdint.h>

#include "esp_err.h"
#include "hal/eth_types.h"

#ifdef __cplusplus
extern "C" {
#endif

void initialize_ethernet(void);

eth_speed_t ethernet_get_mgmt_if_link_speed(void);
void ethernet_get_mac(uint8_t mac[6]);

#ifdef __cplusplus
}
#endif
