#include "sdkconfig.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "linenoise/linenoise.h"
#include "esp_console.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

#include "cmd_sniffer.h"
#include "cmd_pcap.h"
#include "ethernet.h"
#include "sdcard.h"
#include "spi.h"

#if CONFIG_SNIFFER_STORE_HISTORY
#define HISTORY_MOUNT_POINT "/data"
#define HISTORY_FILE_PATH HISTORY_MOUNT_POINT "/history.txt"
#endif

static const char TAG[] = "MAIN";

/* Initialize filesystem */
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

void app_main(void)
{
    initialize_nvs();
    initialize_filesystem();

    initialize_spi();

    /*--- Initialize Network ---*/
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    /* Initialize WiFi */
    initialize_wifi();
    /* Initialize Ethernet */
    initialize_ethernet();

    /*--- Initialize Console ---*/
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
#if CONFIG_SNIFFER_STORE_HISTORY
    repl_config.history_save_path = HISTORY_FILE_PATH;
#endif
    repl_config.prompt = "cli>";

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
