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
HAL_StatusTypeDef APP_WiFi_Platform_Cmd52Read(uint8_t functionNumber, uint32_t address, uint8_t *value);
HAL_StatusTypeDef APP_WiFi_Platform_Cmd52Write(uint8_t functionNumber, uint32_t address, uint8_t value);
HAL_StatusTypeDef APP_WiFi_Platform_ProbeCccr(void);
HAL_StatusTypeDef APP_WiFi_Platform_EnableFunction1(void);
HAL_StatusTypeDef APP_WiFi_Platform_ConfigureBus(void);
HAL_StatusTypeDef APP_WiFi_Platform_Cmd53Read(uint8_t functionNumber, uint32_t address, uint8_t *data, uint16_t dataLength);
HAL_StatusTypeDef APP_WiFi_Platform_RunCmd53SmokeTest(void);
HAL_StatusTypeDef APP_WiFi_Platform_Fn1Read8(uint32_t address, uint8_t *value);
HAL_StatusTypeDef APP_WiFi_Platform_Fn1Write8(uint32_t address, uint8_t value);
HAL_StatusTypeDef APP_WiFi_Platform_RequestAlpClock(void);
HAL_StatusTypeDef APP_WiFi_Platform_SetBackplaneWindow(uint32_t address);
HAL_StatusTypeDef APP_WiFi_Platform_BackplaneRead(uint32_t address, uint8_t *data, uint16_t dataLength);
HAL_StatusTypeDef APP_WiFi_Platform_BackplaneRead32(uint32_t address, uint32_t *value);
HAL_StatusTypeDef APP_WiFi_Platform_BackplaneWrite(uint32_t address, const uint8_t *data, uint16_t dataLength);
HAL_StatusTypeDef APP_WiFi_Platform_BackplaneWrite32(uint32_t address, uint32_t value);
HAL_StatusTypeDef APP_WiFi_Platform_RunBackplaneSmokeTest(void);
HAL_StatusTypeDef APP_WiFi_Platform_RunBackplaneWriteSmokeTest(void);
HAL_StatusTypeDef APP_WiFi_Platform_RequestHtClock(void);
HAL_StatusTypeDef APP_WiFi_Platform_EnableFunction2(void);
HAL_StatusTypeDef APP_WiFi_Platform_ConfigureInterruptPath(void);
HAL_StatusTypeDef APP_WiFi_Platform_ProbeFirmwareResources(void);
uint32_t APP_WiFi_Platform_GetSdioInterruptCount(void);
uint32_t APP_WiFi_Platform_GetLastSdioStatus(void);
uint32_t APP_WiFi_Platform_GetLastSdioError(void);
uint32_t APP_WiFi_Platform_GetSdioOcr(void);
uint16_t APP_WiFi_Platform_GetSdioRca(void);
uint32_t APP_WiFi_Platform_GetLastCmd53Word(void);
uint32_t APP_WiFi_Platform_GetLastBackplaneWord(void);
uint32_t APP_WiFi_Platform_GetBackplaneWindowBase(void);
uint8_t APP_WiFi_Platform_GetChipClockCsr(void);
uint8_t APP_WiFi_Platform_GetCccrInterruptEnable(void);
uint8_t APP_WiFi_Platform_GetSepInterruptControl(void);
uint8_t APP_WiFi_Platform_GetCccrIoEnable(void);
uint8_t APP_WiFi_Platform_GetCccrBusControl(void);
uint8_t APP_WiFi_Platform_GetCccrRevision(void);
uint8_t APP_WiFi_Platform_GetCccrSdRevision(void);
uint8_t APP_WiFi_Platform_GetCccrIoReady(void);
uint8_t APP_WiFi_Platform_GetCccrCapabilities(void);
uint32_t APP_WiFi_Platform_GetHostInterruptMask(void);
uint32_t APP_WiFi_Platform_GetFunctionInterruptMask(void);
uint32_t APP_WiFi_Platform_GetFirmwareSize(void);
uint32_t APP_WiFi_Platform_GetFirmwareEntryWord(void);
uint32_t APP_WiFi_Platform_GetNvramSize(void);
uint32_t APP_WiFi_Platform_GetNvramStagingAddress(void);
uint32_t APP_WiFi_Platform_GetNvramTrailerWord(void);
void APP_WiFi_Platform_SDMMC_IRQHandler(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_WIFI_PLATFORM_H */
