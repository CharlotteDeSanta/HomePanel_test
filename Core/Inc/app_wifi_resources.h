#ifndef APP_WIFI_RESOURCES_H
#define APP_WIFI_RESOURCES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "stm32h7xx_hal.h"

void APP_WiFi_Resources_Init(void);
HAL_StatusTypeDef APP_WiFi_Resources_GetFirmware(const uint8_t **data, uint32_t *size);
HAL_StatusTypeDef APP_WiFi_Resources_GetNvram(const uint8_t **data, uint32_t *size);
HAL_StatusTypeDef APP_WiFi_Resources_ReadFirmware(uint32_t offset, uint8_t *buffer, uint32_t bufferSize, uint32_t *sizeRead);
HAL_StatusTypeDef APP_WiFi_Resources_ReadNvram(uint32_t offset, uint8_t *buffer, uint32_t bufferSize, uint32_t *sizeRead);

#ifdef __cplusplus
}
#endif

#endif /* APP_WIFI_RESOURCES_H */
