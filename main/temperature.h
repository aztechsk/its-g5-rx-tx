#pragma once

#ifdef CONFIG_ENABLE_TEMPERATURE

#ifdef __cplusplus
extern "C" {
#endif

void temperature_init(void);
float temperature_get(void);

#ifdef __cplusplus
}
#endif

#endif // CONFIG_ENABLE_TEMPERATURE
