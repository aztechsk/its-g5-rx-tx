#include "sdkconfig.h"

#include <stdint.h>
#include <stdbool.h>

#include "esp_event.h"
#include "esp_timer.h"

#include "led_indicator.h"

#include "ethernet.h"
#include "events.h"

#include "led.h"

led_indicator_handle_t led_handle;
bool sniffer_running;
bool mqtt_connected;

#define LED_IRGB(i, r, g, b) SET_IRGB(i, r, g, b)

#define LED_SYSTEM  0
#define LED_SNIFFER 1
#define LED_ETH     2
#define LED_MQTT    3
#define LED_CITS    4

#define LED_ETH_COLOR_100M LED_IRGB(LED_ETH,    0, 0xFF, 0)
#define LED_ETH_COLOR_10M  LED_IRGB(LED_ETH, 0xFF, 0xA0, 0)

static uint32_t system_led_state  = LED_IRGB(LED_SYSTEM,  0xFF, 0xFF, 0xFF);
static uint32_t sniffer_led_state = LED_IRGB(LED_SNIFFER, 0xFF,    0,    0);
static uint32_t eth_led_state     = LED_IRGB(LED_ETH,        0,    0,    0);
static uint32_t cits_led_state    = LED_IRGB(LED_CITS,       0,    0,    0);
static uint32_t mqtt_led_state    = LED_IRGB(LED_MQTT,       0,    0,    0);

bool system_led_blink_state;

esp_timer_handle_t system_led_timer_handle;
esp_timer_handle_t cits_led_timer_handle;
esp_timer_handle_t eth_led_timer_handle;

eth_speed_t eth_speed;
bool eth_link_state;
bool eth_led_blink_state;

static void set_eth_led_disconnected(void)
{
    esp_timer_stop_blocking(eth_led_timer_handle, 10 / portTICK_PERIOD_MS);

    eth_led_state = LED_IRGB(LED_ETH, 0, 0, 0);
    led_indicator_set_rgb(led_handle, mqtt_led_state);
}

static void set_eth_led_connected(void)
{
    esp_timer_stop_blocking(eth_led_timer_handle, 10 / portTICK_PERIOD_MS);

    eth_led_blink_state = false;
    eth_led_state = LED_IRGB(LED_ETH, 0, 0, 0);
    led_indicator_set_rgb(led_handle, eth_led_state);

    esp_timer_start_periodic(eth_led_timer_handle, 500000);
}

static void set_eth_led_connected_with_ip(void)
{
    esp_timer_stop_blocking(eth_led_timer_handle, 10 / portTICK_PERIOD_MS);

    eth_led_state = eth_speed == ETH_SPEED_100M ? LED_ETH_COLOR_100M : LED_ETH_COLOR_10M;
    led_indicator_set_rgb(led_handle, eth_led_state);
}

static void set_mqtt_led_destroyed(void)
{
    mqtt_led_state = LED_IRGB(LED_MQTT, 0, 0, 0);
    led_indicator_set_rgb(led_handle, mqtt_led_state);
}

static void set_mqtt_led_disconnected(void)
{
    mqtt_led_state = LED_IRGB(LED_MQTT, 0xFF, 0xFF, 0);
    led_indicator_set_rgb(led_handle, mqtt_led_state);
}

static void set_mqtt_led_connected(void)
{
    mqtt_led_state = LED_IRGB(LED_MQTT, 0, 0xFF, 0);
    led_indicator_set_rgb(led_handle, mqtt_led_state);
}

static void set_cits_led_idle(void)
{
    cits_led_state = LED_IRGB(LED_CITS, 0, 0, 0);
    led_indicator_set_rgb(led_handle, cits_led_state);
}

static void set_cits_led_active(void)
{
    cits_led_state = mqtt_connected ? LED_IRGB(LED_CITS, 0, 0, 0xFF) : LED_IRGB(LED_CITS, 0xFF, 0xA0, 0);
    led_indicator_set_rgb(led_handle, cits_led_state);
}

static void set_sniffer_led_stopped(void)
{
    sniffer_led_state = LED_IRGB(LED_SNIFFER, 0xFF, 0, 0);
    led_indicator_set_rgb(led_handle, sniffer_led_state);
}

static void set_sniffer_led_running(void)
{
    sniffer_led_state = LED_IRGB(LED_SNIFFER, 0, 0xFF, 0);
    led_indicator_set_rgb(led_handle, sniffer_led_state);
}

static void app_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    switch (event_id)
    {
    case APP_ETHERNET_MGMT_INTERFACE_CONNECTED:
        eth_link_state = true;
        eth_speed = ethernet_get_mgmt_if_link_speed();
        set_eth_led_connected();
        break;
    case APP_ETHERNET_MGMT_INTERFACE_GOT_IP:
        set_eth_led_connected_with_ip();
        set_mqtt_led_disconnected();
        break;
    case APP_ETHERNET_MGMT_INTERFACE_LOST_IP:
        if (eth_link_state)
            set_eth_led_connected();
        set_mqtt_led_destroyed();
        break;
    case APP_ETHERNET_MGMT_INTERFACE_DISCONNECTED:
        eth_link_state = false;
        set_eth_led_disconnected();
        set_mqtt_led_destroyed();
        break;
    }
}

static void sniffer_event_handler(void* arg, esp_event_base_t event_base,
                                  int32_t event_id, void* event_data)
{
    switch (event_id)
    {
    case SNIFFER_RECEIVED_PACKET:
        set_cits_led_active();
        if (esp_timer_restart(cits_led_timer_handle, 50000) == ESP_ERR_INVALID_STATE)
        {
            esp_timer_start_once(cits_led_timer_handle, 50000);
        }
        break;
    case SNIFFER_STARTED:
        sniffer_running = true;
        set_sniffer_led_running();
        set_cits_led_idle();
        break;
    case SNIFFER_STOPPED:
        sniffer_running = false;
        set_sniffer_led_stopped();
        set_cits_led_idle();
        break;
    }
}

static void mqtt_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    switch (event_id)
    {
    case MQTT_CONNECTED:
        mqtt_connected = true;
        set_mqtt_led_connected();
        break;
    case MQTT_DISCONNECTED:
        mqtt_connected = false;
        if (eth_link_state)
            set_mqtt_led_disconnected();
        else
            set_mqtt_led_destroyed();
        break;
    }
}

static void system_led_timer_cb(void *)
{
    led_indicator_set_rgb(led_handle, system_led_blink_state ?
                              LED_IRGB(LED_SYSTEM, 0xFF, 0xFF, 0xFF) :
                              LED_IRGB(LED_SYSTEM, 0, 0, 0));
    system_led_blink_state = !system_led_blink_state;
}

static void eth_led_timer_cb(void *)
{
    eth_led_state = eth_led_blink_state ?
                (eth_speed == ETH_SPEED_100M ? LED_ETH_COLOR_100M : LED_ETH_COLOR_10M) :
                LED_IRGB(LED_ETH, 0, 0, 0);
    led_indicator_set_rgb(led_handle, eth_led_state);

    eth_led_blink_state = !eth_led_blink_state;
}

static void cits_led_timer_cb(void *)
{
    set_cits_led_idle();
}

void led_update(void)
{
    led_indicator_set_rgb(led_handle, system_led_state);
    led_indicator_set_rgb(led_handle, sniffer_led_state);
    led_indicator_set_rgb(led_handle, eth_led_state);
    led_indicator_set_rgb(led_handle, mqtt_led_state);
    led_indicator_set_rgb(led_handle, cits_led_state);

    esp_timer_start_periodic(system_led_timer_handle, 1000000);
}

void led_init(void)
{
    led_indicator_strips_config_t strips_config = {
        .led_strip_cfg = {
            .strip_gpio_num = CONFIG_LEDSTRIP_PIN,
            .max_leds = 5,
            .led_pixel_format = LED_PIXEL_FORMAT_GRB,
            .led_model = LED_MODEL_WS2812
        },
        .led_strip_driver = LED_STRIP_RMT,
        .led_strip_rmt_cfg = {0}
    };
    led_indicator_config_t led_config = {
        .mode = LED_STRIPS_MODE,
        .led_indicator_strips_config = &strips_config,
    };
    led_handle = led_indicator_create(&led_config);

    esp_timer_create_args_t create_args = {
        .callback = cits_led_timer_cb,
        .arg = NULL,
        .name = "cits_led"
    };
    ESP_ERROR_CHECK(esp_timer_create(&create_args, &cits_led_timer_handle));

    create_args.callback = eth_led_timer_cb;
    create_args.name = "eth_led";
    ESP_ERROR_CHECK(esp_timer_create(&create_args, &eth_led_timer_handle));

    create_args.callback = system_led_timer_cb;
    create_args.name = "system_led";
    ESP_ERROR_CHECK(esp_timer_create(&create_args, &system_led_timer_handle));

    ESP_ERROR_CHECK(esp_event_handler_register(SNIFFER_EVENT_BASE, ESP_EVENT_ANY_ID, sniffer_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(MQTT_EVENT_BASE, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(APP_EVENT_BASE, ESP_EVENT_ANY_ID, app_event_handler, NULL));

    led_indicator_set_rgb(led_handle, LED_IRGB(0, 0xFF,    0,    0));
    led_indicator_set_rgb(led_handle, LED_IRGB(1, 0xFF, 0xFF,    0));
    led_indicator_set_rgb(led_handle, LED_IRGB(2,    0, 0xFF,    0));
    led_indicator_set_rgb(led_handle, LED_IRGB(3,    0,    0, 0xFF));
    led_indicator_set_rgb(led_handle, LED_IRGB(4, 0xFF,    0, 0xFF));
}
