#include "sdkconfig.h"

#include "esp_log.h"
#include "mqtt_client.h"

#include "cmd_sniffer.h"
#include "config.h"
#include "events.h"

#include "mqtt.h"

static const char TAG[] = "MQTT";

static esp_mqtt_client_handle_t client;

static char topic_prefix[96];
static char command_topic[128];
static char packet_topic[128];

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);

    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    // your_context_t *context = event->context;
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            msg_id = esp_mqtt_client_subscribe(client, command_topic, 0);
            ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            break;

        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT_EVENT_DATA");
            printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
            printf("DATA=%.*s\r\n", event->data_len, event->data);
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
            if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
                ESP_LOGI(TAG, "Last error code reported from esp-tls: 0x%x", event->error_handle->esp_tls_last_esp_err);
                ESP_LOGI(TAG, "Last tls stack error number: 0x%x", event->error_handle->esp_tls_stack_err);
                ESP_LOGI(TAG, "Last captured errno : %d (%s)",  event->error_handle->esp_transport_sock_errno,
                                                                strerror(event->error_handle->esp_transport_sock_errno));
            } else if (event->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
                ESP_LOGI(TAG, "Connection refused error: 0x%x", event->error_handle->connect_return_code);
            } else {
                ESP_LOGW(TAG, "Unknown error type: 0x%x", event->error_handle->error_type);
            }
            break;
        default:
            ESP_LOGI(TAG, "Other event id:%d", event->event_id);
            break;
    }
}

static int make_topic_prefix()
{
    size_t size = sizeof(topic_prefix);
    strcpy(topic_prefix, "its/");
    size -= 4;

    esp_err_t res = config_get_str(CONFIG_INDEX_NODEID, topic_prefix + 4, &size);
    if (res != ESP_OK)
    {
        ESP_LOGE(TAG, "Could not get node ID from config: %s", esp_err_to_name(res));
        return res;
    }

    topic_prefix[4 + size - 1] = '/';
    topic_prefix[4 + size] = '\0';

    return ESP_OK;
}

void mqtt_start(void)
{
    if (client)
    {
        ESP_LOGW(TAG, "MQTT already started");
        return;
    }

    make_topic_prefix();

    strcpy(command_topic, topic_prefix);
    strcat(command_topic, "command");

    strcpy(packet_topic, topic_prefix);
    strcat(packet_topic, "packet");

    esp_mqtt_client_config_t mqtt_cfg = {0};

    char mqtt_uri[CONFIG_MQTT_URI_BUFFER_SIZE];
    size_t size = sizeof(mqtt_uri);
    esp_err_t res = config_get_str(CONFIG_INDEX_MQTT_URI, mqtt_uri, &size);
    if (res != ESP_OK)
    {
        ESP_LOGE(TAG, "Could not get MQTT URI from config: %s", esp_err_to_name(res));
        return;
    }

    mqtt_cfg.broker.address.uri = mqtt_uri;

    esp_mqtt_client_handle_t client_ = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client_, ESP_EVENT_ANY_ID, mqtt_event_handler, client_);
    esp_mqtt_client_start(client_);

    client = client_;
    ESP_LOGI(TAG, "MQTT started");
}

void mqtt_stop(void)
{
    if (!client)
    {
        ESP_LOGW(TAG, "Attempted to stop MQTT while not running");
        return;
    }

    esp_mqtt_client_stop(client);

    esp_mqtt_client_handle_t client_ = client;
    client = NULL;
    esp_mqtt_client_destroy(client_);
    ESP_LOGI(TAG, "MQTT stopped");
}

static void app_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    switch (event_id)
    {
    case APP_ETHERNET_MGMT_INTERFACE_CONNECTED:
        mqtt_start();
        break;
    case APP_ETHERNET_MGMT_INTERFACE_DISCONNECTED:
        mqtt_stop();
        break;
    }
}

static void sniffer_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    if (!client)
        return;

    switch (event_id)
    {
    case SNIFFER_GOT_FRAME:
        sniffer_combined_info_t *info = event_data;
        esp_mqtt_client_publish(client, packet_topic, (const char *)info->payload, info->info.length, 0, 0);
    }
}

void mqtt_init(void)
{
    esp_event_handler_register(APP_EVENT_BASE, ESP_EVENT_ANY_ID, app_event_handler, NULL);
    esp_event_handler_register(SNIFFER_EVENT_BASE, ESP_EVENT_ANY_ID, sniffer_event_handler, NULL);
}
