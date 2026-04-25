#ifndef APP_WIFI_PLATFORM_H
#define APP_WIFI_PLATFORM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "stm32h7xx_hal.h"

void APP_WiFi_Platform_Init(void);
void APP_WiFi_Platform_SetResetPin(GPIO_PinState pinState);
HAL_StatusTypeDef APP_WiFi_Platform_SdioHostInit(void);
HAL_StatusTypeDef APP_WiFi_Platform_SdioEnumerate(void);
uint32_t APP_WiFi_Platform_GetSdioInterruptCount(void);
uint32_t APP_WiFi_Platform_GetLastSdioStatus(void);
uint32_t APP_WiFi_Platform_GetLastSdioError(void);
uint32_t APP_WiFi_Platform_GetSdioOcr(void);
uint16_t APP_WiFi_Platform_GetSdioRca(void);
void APP_WiFi_Platform_SDMMC_IRQHandler(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_WIFI_PLATFORM_H */
