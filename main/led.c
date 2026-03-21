#include "esp_event.h"
#include "esp_timer.h"

#include "led_indicator.h"

#include "events.h"

#include "led.h"

esp_timer_handle_t timer_handle;
led_indicator_handle_t led_handle;

static void packet_event_handler(void* arg, esp_event_base_t event_base,
                                 int32_t event_id, void* event_data)
{
    led_indicator_set_rgb(led_handle, 0xFF);
    esp_timer_start_once(timer_handle, 50000);
}

static void timer_cb(void *)
{
    led_indicator_set_rgb(led_handle, 0);
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

    ESP_ERROR_CHECK(esp_event_handler_register(SNIFFER_EVENT_BASE, PACKET_RECEIVED, packet_event_handler, NULL));

}
