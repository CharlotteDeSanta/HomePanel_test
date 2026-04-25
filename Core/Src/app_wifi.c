#include "app_wifi.h"

#include "app_wifi_platform.h"
#include "main.h"

#define APP_WIFI_STACK_WAIT_MS          50U
#define APP_WIFI_RESET_ASSERT_MS        20U
#define APP_WIFI_RESET_RELEASE_GUARD_MS 50U
#define APP_WIFI_MODULE_SETTLE_MS       200U
#define APP_WIFI_POLL_MS                1000U

static volatile APP_WiFiState_t g_wifiState = APP_WIFI_STATE_IDLE;
static volatile uint32_t g_wifiOobInterruptCount = 0U;

static void APP_WiFi_SetState(APP_WiFiState_t nextState)
{
  g_wifiState = nextState;
}

void APP_WiFi_Init(void)
{
  g_wifiOobInterruptCount = 0U;
  APP_WiFi_Platform_Init();
  APP_WiFi_SetState(APP_WIFI_STATE_IDLE);
}

APP_WiFiState_t APP_WiFi_GetState(void)
{
  return g_wifiState;
}

uint32_t APP_WiFi_GetOobInterruptCount(void)
{
  return g_wifiOobInterruptCount;
}

void APP_WiFi_HandleOobInterrupt(uint16_t gpioPin)
{
  if (gpioPin == WIFI_OOB_IRQ_Pin)
  {
    g_wifiOobInterruptCount++;
  }
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  APP_WiFi_HandleOobInterrupt(GPIO_Pin);
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
        APP_WiFi_Platform_SetResetPin(GPIO_PIN_SET);
        APP_WiFi_SetState(APP_WIFI_STATE_WAIT_STACK);
        osDelay(APP_WIFI_STACK_WAIT_MS);
        break;

      case APP_WIFI_STATE_WAIT_STACK:
        /*
         * FreeRTOS is already running here. When we start integrating the
         * AP6181 stack, any one-time scheduler-dependent initialization can
         * move into this state.
         */
        APP_WiFi_SetState(APP_WIFI_STATE_RESET_ASSERT);
        break;

      case APP_WIFI_STATE_RESET_ASSERT:
        /*
         * AP6181 needs a clean hardware reset before we attempt any SDIO/WWD
         * bring-up. We do that here instead of inside board init so the whole
         * sequence lives in the dedicated Wi-Fi task.
         */
        APP_WiFi_Platform_SetResetPin(GPIO_PIN_RESET);
        APP_WiFi_SetState(APP_WIFI_STATE_RESET_RELEASE);
        osDelay(APP_WIFI_RESET_ASSERT_MS);
        break;

      case APP_WIFI_STATE_RESET_RELEASE:
        APP_WiFi_Platform_SetResetPin(GPIO_PIN_SET);
        APP_WiFi_SetState(APP_WIFI_STATE_MODULE_SETTLE);
        osDelay(APP_WIFI_RESET_RELEASE_GUARD_MS);
        break;

      case APP_WIFI_STATE_MODULE_SETTLE:
        /*
         * Keep a short settle window after reset deassertion. We intentionally
         * stop here for now: AP6181 is not a standard SD card, so we do not
         * call MX_SDMMC1_SD_Init()/HAL_SD_Init() from this task. The future
         * WWD/SDIO bring-up will replace the BRINGUP_PENDING placeholder.
         */
        APP_WiFi_SetState(APP_WIFI_STATE_BRINGUP_PENDING);
        osDelay(APP_WIFI_MODULE_SETTLE_MS);
        break;

      case APP_WIFI_STATE_BRINGUP_PENDING:
        if (APP_WiFi_Platform_SdioHostInit() == HAL_OK)
        {
          APP_WiFi_SetState(APP_WIFI_STATE_SDIO_HOST_READY);
          osDelay(APP_WIFI_STACK_WAIT_MS);
        }
        else
        {
          APP_WiFi_SetState(APP_WIFI_STATE_ERROR);
          osDelay(APP_WIFI_POLL_MS);
        }
        break;

      case APP_WIFI_STATE_SDIO_HOST_READY:
      case APP_WIFI_STATE_READY:
      case APP_WIFI_STATE_ERROR:
      default:
        osDelay(APP_WIFI_POLL_MS);
        break;
    }
  }
}
