/* Sniffer example.
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "sdkconfig.h"
#include "linenoise/linenoise.h"
#include "esp_console.h"
#include "esp_event.h"
#include "esp_vfs_fat.h"
#include "esp_wifi.h"
#include "ethernet_init.h"
#include "esp_err.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "cmd_sniffer.h"
#include "cmd_pcap.h"
#include "sdcard.h"

#if CONFIG_SNIFFER_STORE_HISTORY
#define HISTORY_MOUNT_POINT "/data"
#define HISTORY_FILE_PATH HISTORY_MOUNT_POINT "/history.txt"
#endif

static const char *TAG = "example";

#if CONFIG_SNIFFER_STORE_HISTORY
/* Initialize filesystem for command history store */
static void initialize_filesystem(void)
{
    static wl_handle_t wl_handle;
    const esp_vfs_fat_mount_config_t mount_config = {
        .max_files = 4,
        .format_if_mount_failed = true
    };
    esp_err_t err = esp_vfs_fat_spiflash_mount_rw_wl(HISTORY_MOUNT_POINT, "storage", &mount_config, &wl_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount FATFS (%s)", esp_err_to_name(err));
        return;
    }
}
#endif

static void initialize_nvs(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
}

/* Initialize wifi with tcp/ip adapter */
static void initialize_wifi(void)
{
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_NULL));
}

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
        printf("\n");
        ESP_LOGI(TAG, "Ethernet Link Up");
        ESP_LOGI(TAG, "Ethernet HW Addr %02x:%02x:%02x:%02x:%02x:%02x",
                 mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
        break;
    case ETHERNET_EVENT_DISCONNECTED:
        printf("\n");
        ESP_LOGI(TAG, "Ethernet Link Down");
        break;
    case ETHERNET_EVENT_START:
        printf("\n");
        ESP_LOGI(TAG, "Ethernet Started");
        break;
    case ETHERNET_EVENT_STOP:
        printf("\n");
        ESP_LOGI(TAG, "Ethernet Stopped");
        break;
    default:
        break;
    }
}

static void initialize_eth(void)
{
    // Initialize Ethernet driver
    uint8_t eth_port_cnt = 0;
    esp_eth_handle_t *eth_handles;
    ESP_ERROR_CHECK(ethernet_init_all(&eth_handles, &eth_port_cnt));

    if (eth_port_cnt > 0) {
        // Register user defined event handlers
        ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &eth_event_handler, NULL));

        for (uint32_t i = 0; i < eth_port_cnt; i++) {
            /* start Ethernet driver state machine */
            ESP_ERROR_CHECK(esp_eth_start(eth_handles[i]));
            /* Register Ethernet interface to could be used by sniffer */
            ESP_ERROR_CHECK(sniffer_reg_eth_intf(eth_handles[i]));
        }
    }
}

#ifdef CONFIG_ETHERNET_SPI_SUPPORT
#define PIN_NUM_MOSI CONFIG_ETHERNET_SPI_MOSI_GPIO
#define PIN_NUM_MISO CONFIG_ETHERNET_SPI_MISO_GPIO
#define PIN_NUM_CLK CONFIG_ETHERNET_SPI_SCLK_GPIO
#elifdef CONFIG_SNIFFER_SD_SPI_MODE
#define PIN_NUM_MISO CONFIG_SNIFFER_PIN_MISO
#define PIN_NUM_MOSI CONFIG_SNIFFER_PIN_MOSI
#define PIN_NUM_CLK  CONFIG_SNIFFER_PIN_CLK
#endif

// Initialize SPI bus common to SD card and Ethernet
void initialize_spi(void)
{
#if CONFIG_SNIFFER_SD_SPI_MODE || CONFIG_ETHERNET_SPI_SUPPORT
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = PIN_NUM_MISO,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };

    // TODO: maybe make SPI host configurable
    int ret = spi_bus_initialize(SDSPI_DEFAULT_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK)
        ESP_LOGE(TAG, "Failed to initialize bus.");
#endif
}

void app_main(void)
{
    initialize_nvs();

    initialize_spi();

    /*--- Initialize Network ---*/
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    /* Initialize WiFi */
    initialize_wifi();
    /* Initialize Ethernet */
    initialize_eth();

    /*--- Initialize Console ---*/
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
#if CONFIG_SNIFFER_STORE_HISTORY
    initialize_filesystem();
    repl_config.history_save_path = HISTORY_FILE_PATH;
#endif
    repl_config.prompt = "sniffer>";

    // install console REPL environment
#if CONFIG_ESP_CONSOLE_UART
    esp_console_dev_uart_config_t uart_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_uart(&uart_config, &repl_config, &repl));
#elif CONFIG_ESP_CONSOLE_USB_CDC
    esp_console_dev_usb_cdc_config_t cdc_config = ESP_CONSOLE_DEV_CDC_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_usb_cdc(&cdc_config, &repl_config, &repl));
#elif CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
    esp_console_dev_usb_serial_jtag_config_t usbjtag_config = ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_usb_serial_jtag(&usbjtag_config, &repl_config, &repl));
#endif

    /* Register commands */
#if CONFIG_ENABLE_SD
    register_mount();
    register_unmount();
#endif
    register_sniffer_cmd();
    register_pcap_cmd();

    // start console REPL
    ESP_ERROR_CHECK(esp_console_start_repl(repl));
}
