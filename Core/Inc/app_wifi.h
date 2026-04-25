#ifndef APP_WIFI_H
#define APP_WIFI_H

#ifdef __cplusplus
extern "C" {
#endif

#include "cmsis_os2.h"

typedef enum
{
  APP_WIFI_STATE_IDLE = 0,
  APP_WIFI_STATE_WAIT_STACK,
  APP_WIFI_STATE_PREPARE_MODULE,
  APP_WIFI_STATE_BRINGUP_PENDING,
  APP_WIFI_STATE_READY,
  APP_WIFI_STATE_ERROR
} APP_WiFiState_t;

void APP_WiFi_Init(void);
void APP_WiFi_Task(void *argument);
APP_WiFiState_t APP_WiFi_GetState(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_WIFI_H */
