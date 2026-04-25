#include "app_wifi.h"

static APP_WiFiState_t g_wifiState = APP_WIFI_STATE_IDLE;

void APP_WiFi_Init(void)
{
  g_wifiState = APP_WIFI_STATE_IDLE;
}

APP_WiFiState_t APP_WiFi_GetState(void)
{
  return g_wifiState;
}

void APP_WiFi_Task(void *argument)
{
  (void)argument;

  /*
   * This task is the future home of the AP6181 bring-up path:
   * SDMMC/WWD init, join, DHCP, and socket transport should stay here
   * instead of running inside the GUI task.
   */
  for (;;)
  {
    switch (g_wifiState)
    {
      case APP_WIFI_STATE_IDLE:
        g_wifiState = APP_WIFI_STATE_WAIT_STACK;
        osDelay(50);
        break;

      case APP_WIFI_STATE_WAIT_STACK:
        /*
         * FreeRTOS is already running here. When we start integrating the
         * AP6181 stack, any one-time scheduler-dependent initialization can
         * move into this state.
         */
        g_wifiState = APP_WIFI_STATE_PREPARE_MODULE;
        osDelay(50);
        break;

      case APP_WIFI_STATE_PREPARE_MODULE:
        /*
         * Placeholder for AP6181 reset pin setup, OOB interrupt enable,
         * SDMMC bring-up, WWD startup, and LwIP bootstrap.
         */
        g_wifiState = APP_WIFI_STATE_BRINGUP_PENDING;
        osDelay(100);
        break;

      case APP_WIFI_STATE_BRINGUP_PENDING:
      case APP_WIFI_STATE_READY:
      case APP_WIFI_STATE_ERROR:
      default:
        osDelay(1000);
        break;
    }
  }
}
