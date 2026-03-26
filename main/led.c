#include "esp_event.h"
#include "esp_timer.h"

#include "led_indicator.h"

#include "events.h"

#include "led.h"

esp_timer_handle_t timer_handle;
led_indicator_handle_t led_handle;
bool sniffer_running;
bool mqtt_connected;

static void set_cits_led_idle(void)
{
    led_indicator_set_rgb(led_handle, sniffer_running ? SET_IRGB(0, 0xFF, 0, 0) : SET_IRGB(0, 0, 0xFF, 0));
}

static void set_cits_led_active(void)
{
    led_indicator_set_rgb(led_handle, mqtt_connected ? SET_IRGB(0, 0xFF, 0xFF, 0xFF) : SET_IRGB(0, 0xFF, 0xFF, 0));
}

static void sniffer_event_handler(void* arg, esp_event_base_t event_base,
                                  int32_t event_id, void* event_data)
{
    switch (event_id)
    {
    case SNIFFER_RECEIVED_PACKET:
        set_cits_led_active();
        esp_timer_start_once(timer_handle, 2000000);
        break;
    case SNIFFER_STARTED:
        sniffer_running = true;
        set_cits_led_idle();
        break;
    case SNIFFER_STOPPED:
        sniffer_running = false;
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
        set_cits_led_idle();
        break;
    case MQTT_DISCONNECTED:
        mqtt_connected = false;
        set_cits_led_idle();
        break;
    }
}

static void timer_cb(void *)
{
    set_cits_led_idle();
}

void led_init()
{
    led_indicator_strips_config_t strips_config = {
        .led_strip_cfg = {
            .strip_gpio_num = 27,
            .max_leds = 1,
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
        .callback = timer_cb,
        .arg = NULL,
        .name = "led_reset"
    };
    ESP_ERROR_CHECK(esp_timer_create(&create_args, &timer_handle));

    ESP_ERROR_CHECK(esp_event_handler_register(SNIFFER_EVENT_BASE, ESP_EVENT_ANY_ID, sniffer_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(MQTT_EVENT_BASE, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL));

    led_indicator_set_rgb(led_handle, SET_IRGB(0, 0, 0xFF, 0));
}
