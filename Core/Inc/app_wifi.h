#ifndef APP_WIFI_H
#define APP_WIFI_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "cmsis_os2.h"

#define APP_WIFI_SCAN_RESULT_CACHE_SIZE 32U

typedef enum
{
  APP_WIFI_STATE_IDLE = 0,
  APP_WIFI_STATE_WAIT_STACK,
  APP_WIFI_STATE_RESET_ASSERT,
  APP_WIFI_STATE_RESET_RELEASE,
  APP_WIFI_STATE_MODULE_SETTLE,
  APP_WIFI_STATE_BRINGUP_PENDING,
  APP_WIFI_STATE_SDIO_HOST_READY,
  APP_WIFI_STATE_SDIO_ENUMERATED,
  APP_WIFI_STATE_CCCR_READY,
  APP_WIFI_STATE_FUNCTION1_READY,
  APP_WIFI_STATE_BUS_READY,
  APP_WIFI_STATE_CMD53_READY,
  APP_WIFI_STATE_CLOCK_READY,
  APP_WIFI_STATE_BACKPLANE_READY,
  APP_WIFI_STATE_HT_CLOCK_READY,
  APP_WIFI_STATE_FUNCTION2_READY,
  APP_WIFI_STATE_INTERRUPTS_READY,
  APP_WIFI_STATE_RESOURCES_READY,
  APP_WIFI_STATE_FIRMWARE_STAGED,
  APP_WIFI_STATE_NVRAM_STAGED,
  APP_WIFI_STATE_ARM_RELEASED,
  APP_WIFI_STATE_FIRMWARE_BOOTED,
  APP_WIFI_STATE_SHARED_READY,
  APP_WIFI_STATE_CONSOLE_READY,
  APP_WIFI_STATE_MAILBOX_READY,
  APP_WIFI_STATE_READY,
  APP_WIFI_STATE_ERROR
} APP_WiFiState_t;

typedef enum
{
  APP_WIFI_LINK_STATE_IDLE = 0,
  APP_WIFI_LINK_STATE_CONNECTING,
  APP_WIFI_LINK_STATE_CONNECTED,
  APP_WIFI_LINK_STATE_FAILED
} APP_WiFiLinkState_t;

typedef struct
{
  uint8_t bssid[6];
  char ssid[33];
  int16_t rssi;
  uint8_t channel;
} APP_WiFiScanResult_t;

void APP_WiFi_Init(void);
void APP_WiFi_Task(void *argument);
APP_WiFiState_t APP_WiFi_GetState(void);
uint32_t APP_WiFi_GetOobInterruptCount(void);
void APP_WiFi_HandleOobInterrupt(uint16_t gpioPin);
uint8_t APP_WiFi_IsScanComplete(void);
uint8_t APP_WiFi_IsScanAborted(void);
uint32_t APP_WiFi_GetCachedScanResultCount(void);
uint32_t APP_WiFi_CopyCachedScanResults(APP_WiFiScanResult_t *results, uint32_t maxResults);
uint8_t APP_WiFi_RequestJoin(const char *ssid, const char *password);
APP_WiFiLinkState_t APP_WiFi_GetLinkState(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_WIFI_H */
