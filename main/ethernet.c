#include "sdkconfig.h"

#include <string.h>

#include "esp_eth.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "ethernet_init.h"
#include "lwip/netif.h"

#include "cmd_sniffer.h"

#include "ethernet.h"

// TODO make configurable?
#define ETH_MANAGEMENT_INTERFACE 0

static const char TAG[] = "ETHERNET";

/** Event handler for Ethernet events */
static void eth_event_handler(void *arg, esp_event_base_t event_base,
                              int32_t event_id, void *event_data)
{
    uint8_t mac_addr[6] = {0};
    /* we can get the ethernet driver handle from event data */
    esp_eth_handle_t eth_handle = *(esp_eth_handle_t *)event_data;

    switch (event_id) {
    case ETHERNET_EVENT_CONNECTED:
        esp_eth_ioctl(eth_handle, ETH_CMD_G_MAC_ADDR, mac_addr);
        ESP_LOGI(TAG, "Ethernet link up, HW addr %02x:%02x:%02x:%02x:%02x:%02x",
                 mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
        break;
    case ETHERNET_EVENT_DISCONNECTED:
        printf("\n");
        ESP_LOGI(TAG, "Ethernet link down");
        break;
    case ETHERNET_EVENT_START:
        printf("\n");
        ESP_LOGI(TAG, "Ethernet started");
        break;
    case ETHERNET_EVENT_STOP:
        printf("\n");
        ESP_LOGI(TAG, "Ethernet stopped");
        break;
    default:
        break;
    }
}

/** Event handler for IP events */
static void ip_event_handler(void *arg, esp_event_base_t event_base,
                              int32_t event_id, void *event_data)
{
    switch (event_id) {
    case IP_EVENT_ETH_GOT_IP:
        {
            ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;

            char ifname[NETIF_NAMESIZE] = {0};
            esp_err_t res = esp_netif_get_netif_impl_name(event->esp_netif, ifname);
            if (res != ESP_OK)
            {
                ESP_LOGE(TAG, "Failed to get netif name: %s", esp_err_to_name(res));
                strcpy(ifname, "unk");
            }

            const esp_netif_ip_info_t *ip_info = &event->ip_info;
            ESP_LOGI(TAG, "Ethernet %s got IP: " IPSTR " netmask: " IPSTR " gw: " IPSTR, ifname, IP2STR(&ip_info->ip), IP2STR(&ip_info->netmask), IP2STR(&ip_info->gw));
            break;
        }
    default:
        break;
    }
}

void initialize_ethernet(void)
{
    // Initialize Ethernet driver
    uint8_t eth_port_cnt = 0;
    esp_eth_handle_t *eth_handles;
    ESP_ERROR_CHECK(ethernet_init_all(&eth_handles, &eth_port_cnt));

    if (eth_port_cnt == 0)
        return;

    // Register user defined event handlers
    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &eth_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &ip_event_handler, NULL));

    for (uint32_t i = 0; i < eth_port_cnt; i++) {
        if (i != ETH_MANAGEMENT_INTERFACE)
        {
            /* start Ethernet driver state machine */
            ESP_ERROR_CHECK(esp_eth_start(eth_handles[i]));
            /* Register Ethernet interface to could be used by sniffer */
            ESP_ERROR_CHECK(sniffer_reg_eth_intf(eth_handles[i]));
        }
        else
        {
            // Start management ethernet interface
            ESP_LOGD(TAG, "Configuring management ethernet interface %d", i);
            ESP_ERROR_CHECK(esp_netif_init());

            esp_netif_inherent_config_t esp_netif_config = ESP_NETIF_INHERENT_DEFAULT_ETH();
            esp_netif_config.route_prio -= i * 5;
            esp_netif_config_t cfg = ESP_NETIF_DEFAULT_ETH();
            cfg.base = &esp_netif_config;
            esp_netif_t *eth_netif = esp_netif_new(&cfg);

            ESP_ERROR_CHECK(esp_netif_attach(eth_netif, esp_eth_new_netif_glue(eth_handles[i])));
            ESP_ERROR_CHECK(esp_eth_start(eth_handles[i]));
        }
    }
}

