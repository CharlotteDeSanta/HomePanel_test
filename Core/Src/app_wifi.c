#include "app_wifi.h"

#include "app_wifi_resources.h"
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
  APP_WiFi_Resources_Init();
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
        if (APP_WiFi_Platform_SdioEnumerate() == HAL_OK)
        {
          APP_WiFi_SetState(APP_WIFI_STATE_SDIO_ENUMERATED);
          osDelay(APP_WIFI_STACK_WAIT_MS);
        }
        else
        {
          APP_WiFi_SetState(APP_WIFI_STATE_ERROR);
          osDelay(APP_WIFI_POLL_MS);
        }
        break;

      case APP_WIFI_STATE_SDIO_ENUMERATED:
        if (APP_WiFi_Platform_ProbeCccr() == HAL_OK)
        {
          /*
           * This is the first real SDIO register-access milestone:
           * the module now responds to CMD52 reads on Fn0/CCCR space.
           * We keep the state separate so the next step can build on this
           * point when enabling functions and moving toward CMD53 transfers.
           */
          APP_WiFi_SetState(APP_WIFI_STATE_CCCR_READY);
          osDelay(APP_WIFI_STACK_WAIT_MS);
        }
        else
        {
          APP_WiFi_SetState(APP_WIFI_STATE_ERROR);
          osDelay(APP_WIFI_POLL_MS);
        }
        break;

      case APP_WIFI_STATE_CCCR_READY:
        if (APP_WiFi_Platform_EnableFunction1() == HAL_OK)
        {
          /*
           * At this point the SDIO backplane function is not only enabled in
           * IOEN, but also reports ready in IORDY. The next steps can safely
           * build on top of a live Function 1 endpoint.
           */
          APP_WiFi_SetState(APP_WIFI_STATE_FUNCTION1_READY);
          osDelay(APP_WIFI_STACK_WAIT_MS);
        }
        else
        {
          APP_WiFi_SetState(APP_WIFI_STATE_ERROR);
          osDelay(APP_WIFI_POLL_MS);
        }
        break;

      case APP_WIFI_STATE_FUNCTION1_READY:
        if (APP_WiFi_Platform_ConfigureBus() == HAL_OK)
        {
          /*
           * Card-side CCCR block size and bus width are now configured, and the
           * local SDMMC host has been switched to match. This is the last
           * lightweight setup step before we start touching CMD53 transfers.
           */
          APP_WiFi_SetState(APP_WIFI_STATE_BUS_READY);
          osDelay(APP_WIFI_STACK_WAIT_MS);
        }
        else
        {
          APP_WiFi_SetState(APP_WIFI_STATE_ERROR);
          osDelay(APP_WIFI_POLL_MS);
        }
        break;

      case APP_WIFI_STATE_BUS_READY:
        if (APP_WiFi_Platform_RunCmd53SmokeTest() == HAL_OK)
        {
          /*
           * First minimal CMD53 smoke test succeeded. We now have proof that
           * the SDIO data path is alive, not just the command path.
           */
          APP_WiFi_SetState(APP_WIFI_STATE_CMD53_READY);
          osDelay(APP_WIFI_STACK_WAIT_MS);
        }
        else
        {
          APP_WiFi_SetState(APP_WIFI_STATE_ERROR);
          osDelay(APP_WIFI_POLL_MS);
        }
        break;

      case APP_WIFI_STATE_CMD53_READY:
        if (APP_WiFi_Platform_RequestAlpClock() == HAL_OK)
        {
          /*
           * The module now acknowledges the ALP clock request through the
           * Function 1 clock CSR path. That gives us a solid base for the
           * upcoming backplane-window and higher-level WWD bring-up steps.
           */
          APP_WiFi_SetState(APP_WIFI_STATE_CLOCK_READY);
          osDelay(APP_WIFI_STACK_WAIT_MS);
        }
        else
        {
          APP_WiFi_SetState(APP_WIFI_STATE_ERROR);
          osDelay(APP_WIFI_POLL_MS);
        }
        break;

      case APP_WIFI_STATE_CLOCK_READY:
        if (APP_WiFi_Platform_RunBackplaneSmokeTest() == HAL_OK)
        {
          /*
           * We can now set a backplane window and pull a real 32-bit word from
           * chipcommon space through Fn1/CMD53. This is the first point where
           * we know the AP6181 backplane path, not just raw SDIO transport, is
           * alive.
           */
          APP_WiFi_SetState(APP_WIFI_STATE_BACKPLANE_READY);
          osDelay(APP_WIFI_STACK_WAIT_MS);
        }
        else
        {
          APP_WiFi_SetState(APP_WIFI_STATE_ERROR);
          osDelay(APP_WIFI_POLL_MS);
        }
        break;

      case APP_WIFI_STATE_BACKPLANE_READY:
        if (APP_WiFi_Platform_RequestHtClock() == HAL_OK)
        {
          /*
           * Unlike the earlier ALP request, HT is kept asserted here so the
           * WLAN backplane stays up for the next stages. This mirrors the
           * normal WWD "bus up" progression more closely.
           */
          APP_WiFi_SetState(APP_WIFI_STATE_HT_CLOCK_READY);
          osDelay(APP_WIFI_STACK_WAIT_MS);
        }
        else
        {
          APP_WiFi_SetState(APP_WIFI_STATE_ERROR);
          osDelay(APP_WIFI_POLL_MS);
        }
        break;

      case APP_WIFI_STATE_HT_CLOCK_READY:
        if (APP_WiFi_Platform_RunBackplaneWriteSmokeTest() == HAL_OK)
        {
          /*
           * The backplane write path now works too. We keep the smoke test
           * conservative by writing a register back with the same value we
           * just read, so the path is exercised without intentionally changing
           * chip configuration.
           */
          APP_WiFi_SetState(APP_WIFI_STATE_READY);
          osDelay(APP_WIFI_STACK_WAIT_MS);
        }
        else
        {
          APP_WiFi_SetState(APP_WIFI_STATE_ERROR);
          osDelay(APP_WIFI_POLL_MS);
        }
        break;

      case APP_WIFI_STATE_READY:
        if (APP_WiFi_Platform_EnableFunction2() == HAL_OK)
        {
          /*
           * Function 2 is the WLAN data/mailbox endpoint. Once it reports
           * ready, we can move on to CCCR/OOB interrupt setup without still
           * being blocked on SDIO function readiness.
           */
          APP_WiFi_SetState(APP_WIFI_STATE_FUNCTION2_READY);
          osDelay(APP_WIFI_STACK_WAIT_MS);
        }
        else
        {
          APP_WiFi_SetState(APP_WIFI_STATE_ERROR);
          osDelay(APP_WIFI_POLL_MS);
        }
        break;

      case APP_WIFI_STATE_FUNCTION2_READY:
        if (APP_WiFi_Platform_ConfigureInterruptPath() == HAL_OK)
        {
          /*
           * CCCR interrupt enables, separate OOB control, and the basic
           * backplane-side interrupt support registers are now programmed.
           * This is a good stable checkpoint before firmware/NVRAM work.
           */
          APP_WiFi_SetState(APP_WIFI_STATE_INTERRUPTS_READY);
          osDelay(APP_WIFI_STACK_WAIT_MS);
        }
        else
        {
          APP_WiFi_SetState(APP_WIFI_STATE_ERROR);
          osDelay(APP_WIFI_POLL_MS);
        }
        break;

      case APP_WIFI_STATE_INTERRUPTS_READY:
        if (APP_WiFi_Platform_ProbeFirmwareResources() == HAL_OK)
        {
          /*
           * Firmware and NVRAM resources are now visible from the application
           * build, and their basic RAM staging layout has been computed. That
           * gives us a clean hand-off point into the actual download logic.
           */
          APP_WiFi_SetState(APP_WIFI_STATE_RESOURCES_READY);
          osDelay(APP_WIFI_STACK_WAIT_MS);
        }
        else
        {
          APP_WiFi_SetState(APP_WIFI_STATE_ERROR);
          osDelay(APP_WIFI_POLL_MS);
        }
        break;

      case APP_WIFI_STATE_RESOURCES_READY:
        if (APP_WiFi_Platform_StageFirmwareImage() == HAL_OK)
        {
          /*
           * The firmware blob has now been copied into the WLAN RAM aperture.
           * We still keep the ARM core halted from a workflow perspective and
           * stage NVRAM separately, so download errors are easier to isolate.
           */
          APP_WiFi_SetState(APP_WIFI_STATE_FIRMWARE_STAGED);
          osDelay(APP_WIFI_STACK_WAIT_MS);
        }
        else
        {
          APP_WiFi_SetState(APP_WIFI_STATE_ERROR);
          osDelay(APP_WIFI_POLL_MS);
        }
        break;

      case APP_WIFI_STATE_FIRMWARE_STAGED:
        if (APP_WiFi_Platform_StageNvramImage() == HAL_OK)
        {
          /*
           * NVRAM and its trailer word are now in place at the top of WLAN
           * RAM. The next step can focus purely on core release and firmware
           * boot, because the download payload itself is already staged.
           */
          APP_WiFi_SetState(APP_WIFI_STATE_NVRAM_STAGED);
          osDelay(APP_WIFI_STACK_WAIT_MS);
        }
        else
        {
          APP_WiFi_SetState(APP_WIFI_STATE_ERROR);
          osDelay(APP_WIFI_POLL_MS);
        }
        break;

      case APP_WIFI_STATE_NVRAM_STAGED:
        if (APP_WiFi_Platform_ReleaseWlanArmCore() == HAL_OK)
        {
          /*
           * The WLAN ARM wrapper has now been taken out of reset using the
           * same minimal sequence the official stack uses. The next state only
           * waits for the core wrapper to report a sane post-release state.
           */
          APP_WiFi_SetState(APP_WIFI_STATE_ARM_RELEASED);
          osDelay(APP_WIFI_STACK_WAIT_MS);
        }
        else
        {
          APP_WiFi_SetState(APP_WIFI_STATE_ERROR);
          osDelay(APP_WIFI_POLL_MS);
        }
        break;

      case APP_WIFI_STATE_ARM_RELEASED:
        if (APP_WiFi_Platform_WaitForFirmwareBoot() == HAL_OK)
        {
          /*
           * At this point the WLAN ARM core wrapper reports clock enabled and
           * not in reset, which is the first practical "firmware booted"
           * checkpoint we can get before pulling in the heavier WWD logic.
           */
          APP_WiFi_SetState(APP_WIFI_STATE_FIRMWARE_BOOTED);
          osDelay(APP_WIFI_STACK_WAIT_MS);
        }
        else
        {
          APP_WiFi_SetState(APP_WIFI_STATE_ERROR);
          osDelay(APP_WIFI_POLL_MS);
        }
        break;

      case APP_WIFI_STATE_FIRMWARE_BOOTED:
        if (APP_WiFi_Platform_ProbeSharedMemory() == HAL_OK)
        {
          /*
           * The firmware has now published a non-zero shared-area pointer at
           * the top of WLAN RAM. That is a much stronger sign of life than the
           * wrapper-only boot check, and it gives us a concrete handhold for
           * later WWD-style shared-memory parsing.
           */
          APP_WiFi_SetState(APP_WIFI_STATE_SHARED_READY);
          osDelay(APP_WIFI_STACK_WAIT_MS);
        }
        else
        {
          APP_WiFi_SetState(APP_WIFI_STATE_ERROR);
          osDelay(APP_WIFI_POLL_MS);
        }
        break;

      case APP_WIFI_STATE_SHARED_READY:
        if (APP_WiFi_Platform_ProbeConsole() == HAL_OK)
        {
          /*
           * The firmware's console structure is now readable through the
           * shared-area pointer, so we have a concrete log-buffer location and
           * cursor state available for later debug-log extraction.
           */
          APP_WiFi_SetState(APP_WIFI_STATE_CONSOLE_READY);
          osDelay(APP_WIFI_STACK_WAIT_MS);
        }
        else
        {
          APP_WiFi_SetState(APP_WIFI_STATE_ERROR);
          osDelay(APP_WIFI_POLL_MS);
        }
        break;

      case APP_WIFI_STATE_CONSOLE_READY:
        if (APP_WiFi_Platform_ProbeMailbox() == HAL_OK)
        {
          /*
          * We can now read the host-facing mailbox and interrupt-status path
           * after firmware boot, on top of the already-parsed shared/console
           * state. That gives the next bring-up steps a stable checkpoint
           * before we start leaning harder on higher-level bus-up assumptions
           * from the vendor stack.
           */
          APP_WiFi_SetState(APP_WIFI_STATE_MAILBOX_READY);
          osDelay(APP_WIFI_STACK_WAIT_MS);
        }
        else
        {
          APP_WiFi_SetState(APP_WIFI_STATE_ERROR);
          osDelay(APP_WIFI_POLL_MS);
        }
        break;

      case APP_WIFI_STATE_MAILBOX_READY:
      case APP_WIFI_STATE_ERROR:
      default:
        osDelay(APP_WIFI_POLL_MS);
        break;
    }
  }
}
