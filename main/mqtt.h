#pragma once

struct sniffer_packet_info_t;

void mqtt_init(void);
void mqtt_handle_packet(sniffer_packet_info_t *packet);
