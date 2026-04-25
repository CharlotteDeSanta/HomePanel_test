#ifndef APP_WIFI_H
#define APP_WIFI_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "cmsis_os2.h"

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
  APP_WIFI_STATE_READY,
  APP_WIFI_STATE_ERROR
} APP_WiFiState_t;

void APP_WiFi_Init(void);
void APP_WiFi_Task(void *argument);
APP_WiFiState_t APP_WiFi_GetState(void);
uint32_t APP_WiFi_GetOobInterruptCount(void);
void APP_WiFi_HandleOobInterrupt(uint16_t gpioPin);

#ifdef __cplusplus
}
#endif

#endif /* APP_WIFI_H */
