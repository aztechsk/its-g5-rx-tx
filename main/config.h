#pragma once

#include "esp_err.h"

#define CONFIG_NODEID_BUFFER_SIZE 64
#define CONFIG_MQTT_URI_BUFFER_SIZE 256

void config_init(void);

typedef enum {
    CONFIG_INDEX_NODEID,
    CONFIG_INDEX_MQTT_URI,
} config_index_t;

esp_err_t config_get_str(config_index_t index, char *out, size_t *size);
esp_err_t config_set_str(config_index_t index, const char *value);


void config_register_commands(void);
