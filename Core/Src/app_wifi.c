#include "app_wifi.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "app_debug_uart.h"
#include "app_wifi_resources.h"
#include "app_wifi_platform.h"
#include "main.h"

#define APP_WIFI_STACK_WAIT_MS          50U
#define APP_WIFI_RESET_ASSERT_MS        20U
#define APP_WIFI_RESET_RELEASE_GUARD_MS 50U
#define APP_WIFI_MODULE_SETTLE_MS       200U
#define APP_WIFI_POLL_MS                1000U
#define APP_WIFI_HEARTBEAT_MS           5000U
#define APP_WIFI_LOG_BUFFER_SIZE        256U

static volatile APP_WiFiState_t g_wifiState = APP_WIFI_STATE_IDLE;
static volatile uint32_t g_wifiOobInterruptCount = 0U;
static uint32_t g_wifiLastHeartbeatTick = 0U;

static const char *APP_WiFi_StateToString(APP_WiFiState_t state);
static void APP_WiFi_Logf(const char *format, ...);
static void APP_WiFi_LogStateDetails(APP_WiFiState_t state);
static void APP_WiFi_LogPeriodicHeartbeat(void);

static void APP_WiFi_SetState(APP_WiFiState_t nextState)
{
  const APP_WiFiState_t previousState = g_wifiState;

  g_wifiState = nextState;

  if (previousState != nextState)
  {
    APP_WiFi_Logf("[wifi] state: %s -> %s\n",
                  APP_WiFi_StateToString(previousState),
                  APP_WiFi_StateToString(nextState));
    APP_WiFi_LogStateDetails(nextState);
  }
}

void APP_WiFi_Init(void)
{
  g_wifiOobInterruptCount = 0U;
  g_wifiLastHeartbeatTick = HAL_GetTick();
  APP_WiFi_Resources_Init();
  APP_WiFi_Platform_Init();
  APP_WiFi_SetState(APP_WIFI_STATE_IDLE);
  APP_WiFi_Logf("[wifi] init complete, debug UART ready on USART1\n");
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

static const char *APP_WiFi_StateToString(APP_WiFiState_t state)
{
  switch (state)
  {
    case APP_WIFI_STATE_IDLE:
      return "IDLE";
    case APP_WIFI_STATE_WAIT_STACK:
      return "WAIT_STACK";
    case APP_WIFI_STATE_RESET_ASSERT:
      return "RESET_ASSERT";
    case APP_WIFI_STATE_RESET_RELEASE:
      return "RESET_RELEASE";
    case APP_WIFI_STATE_MODULE_SETTLE:
      return "MODULE_SETTLE";
    case APP_WIFI_STATE_BRINGUP_PENDING:
      return "BRINGUP_PENDING";
    case APP_WIFI_STATE_SDIO_HOST_READY:
      return "SDIO_HOST_READY";
    case APP_WIFI_STATE_SDIO_ENUMERATED:
      return "SDIO_ENUMERATED";
    case APP_WIFI_STATE_CCCR_READY:
      return "CCCR_READY";
    case APP_WIFI_STATE_FUNCTION1_READY:
      return "FUNCTION1_READY";
    case APP_WIFI_STATE_BUS_READY:
      return "BUS_READY";
    case APP_WIFI_STATE_CMD53_READY:
      return "CMD53_READY";
    case APP_WIFI_STATE_CLOCK_READY:
      return "CLOCK_READY";
    case APP_WIFI_STATE_BACKPLANE_READY:
      return "BACKPLANE_READY";
    case APP_WIFI_STATE_HT_CLOCK_READY:
      return "HT_CLOCK_READY";
    case APP_WIFI_STATE_FUNCTION2_READY:
      return "FUNCTION2_READY";
    case APP_WIFI_STATE_INTERRUPTS_READY:
      return "INTERRUPTS_READY";
    case APP_WIFI_STATE_RESOURCES_READY:
      return "RESOURCES_READY";
    case APP_WIFI_STATE_FIRMWARE_STAGED:
      return "FIRMWARE_STAGED";
    case APP_WIFI_STATE_NVRAM_STAGED:
      return "NVRAM_STAGED";
    case APP_WIFI_STATE_ARM_RELEASED:
      return "ARM_RELEASED";
    case APP_WIFI_STATE_FIRMWARE_BOOTED:
      return "FIRMWARE_BOOTED";
    case APP_WIFI_STATE_SHARED_READY:
      return "SHARED_READY";
    case APP_WIFI_STATE_CONSOLE_READY:
      return "CONSOLE_READY";
    case APP_WIFI_STATE_MAILBOX_READY:
      return "MAILBOX_READY";
    case APP_WIFI_STATE_READY:
      return "READY";
    case APP_WIFI_STATE_ERROR:
      return "ERROR";
    default:
      return "UNKNOWN";
  }
}

static void APP_WiFi_Logf(const char *format, ...)
{
  char logBuffer[APP_WIFI_LOG_BUFFER_SIZE];
  int written = 0;
  va_list args;

  if (format == NULL)
  {
    return;
  }

  va_start(args, format);
  written = vsnprintf(logBuffer, sizeof(logBuffer), format, args);
  va_end(args);

  if (written <= 0)
  {
    return;
  }

  if ((size_t)written >= sizeof(logBuffer))
  {
    logBuffer[sizeof(logBuffer) - 2U] = '\n';
    logBuffer[sizeof(logBuffer) - 1U] = '\0';
  }

  (void)APP_DebugUart_WriteString(logBuffer);
}

static void APP_WiFi_LogStateDetails(APP_WiFiState_t state)
{
  switch (state)
  {
    case APP_WIFI_STATE_SDIO_ENUMERATED:
      APP_WiFi_Logf("[wifi] sdio: OCR=0x%08lX RCA=0x%04X\n",
                    (unsigned long)APP_WiFi_Platform_GetSdioOcr(),
                    (unsigned int)APP_WiFi_Platform_GetSdioRca());
      break;

    case APP_WIFI_STATE_CCCR_READY:
      APP_WiFi_Logf("[wifi] cccr: rev=0x%02X sdrev=0x%02X caps=0x%02X\n",
                    (unsigned int)APP_WiFi_Platform_GetCccrRevision(),
                    (unsigned int)APP_WiFi_Platform_GetCccrSdRevision(),
                    (unsigned int)APP_WiFi_Platform_GetCccrCapabilities());
      break;

    case APP_WIFI_STATE_FUNCTION1_READY:
    case APP_WIFI_STATE_FUNCTION2_READY:
      APP_WiFi_Logf("[wifi] functions: IOEN=0x%02X IORDY=0x%02X\n",
                    (unsigned int)APP_WiFi_Platform_GetCccrIoEnable(),
                    (unsigned int)APP_WiFi_Platform_GetCccrIoReady());
      break;

    case APP_WIFI_STATE_BUS_READY:
      APP_WiFi_Logf("[wifi] bus: BICTRL=0x%02X\n",
                    (unsigned int)APP_WiFi_Platform_GetCccrBusControl());
      break;

    case APP_WIFI_STATE_CMD53_READY:
      APP_WiFi_Logf("[wifi] cmd53: word=0x%08lX\n",
                    (unsigned long)APP_WiFi_Platform_GetLastCmd53Word());
      break;

    case APP_WIFI_STATE_CLOCK_READY:
    case APP_WIFI_STATE_HT_CLOCK_READY:
      APP_WiFi_Logf("[wifi] clock: CSR=0x%02X\n",
                    (unsigned int)APP_WiFi_Platform_GetChipClockCsr());
      break;

    case APP_WIFI_STATE_BACKPLANE_READY:
      APP_WiFi_Logf("[wifi] backplane: window=0x%08lX word=0x%08lX\n",
                    (unsigned long)APP_WiFi_Platform_GetBackplaneWindowBase(),
                    (unsigned long)APP_WiFi_Platform_GetLastBackplaneWord());
      break;

    case APP_WIFI_STATE_INTERRUPTS_READY:
      APP_WiFi_Logf("[wifi] irq: INTEN=0x%02X SEP=0x%02X oob=%lu\n",
                    (unsigned int)APP_WiFi_Platform_GetCccrInterruptEnable(),
                    (unsigned int)APP_WiFi_Platform_GetSepInterruptControl(),
                    (unsigned long)APP_WiFi_GetOobInterruptCount());
      break;

    case APP_WIFI_STATE_RESOURCES_READY:
      APP_WiFi_Logf("[wifi] resources: fw=%luB entry=0x%08lX nvram=%luB nvramAddr=0x%08lX\n",
                    (unsigned long)APP_WiFi_Platform_GetFirmwareSize(),
                    (unsigned long)APP_WiFi_Platform_GetFirmwareEntryWord(),
                    (unsigned long)APP_WiFi_Platform_GetNvramSize(),
                    (unsigned long)APP_WiFi_Platform_GetNvramStagingAddress());
      break;

    case APP_WIFI_STATE_FIRMWARE_STAGED:
      APP_WiFi_Logf("[wifi] staged: firmware=%luB\n",
                    (unsigned long)APP_WiFi_Platform_GetFirmwareBytesStaged());
      break;

    case APP_WIFI_STATE_NVRAM_STAGED:
      APP_WiFi_Logf("[wifi] staged: nvram=%luB trailer=0x%08lX\n",
                    (unsigned long)APP_WiFi_Platform_GetNvramBytesStaged(),
                    (unsigned long)APP_WiFi_Platform_GetNvramTrailerWord());
      break;

    case APP_WIFI_STATE_ARM_RELEASED:
    case APP_WIFI_STATE_FIRMWARE_BOOTED:
      APP_WiFi_Logf("[wifi] wlancore: IOCTRL=0x%02X RESETCTRL=0x%02X\n",
                    (unsigned int)APP_WiFi_Platform_GetWlanCoreIoCtrl(),
                    (unsigned int)APP_WiFi_Platform_GetWlanCoreResetCtrl());
      break;

    case APP_WIFI_STATE_SHARED_READY:
      APP_WiFi_Logf("[wifi] shared: addr=0x%08lX flags=0x%08lX console=0x%08lX fwid=0x%08lX\n",
                    (unsigned long)APP_WiFi_Platform_GetWlanSharedAddress(),
                    (unsigned long)APP_WiFi_Platform_GetWlanSharedFlags(),
                    (unsigned long)APP_WiFi_Platform_GetConsoleAddress(),
                    (unsigned long)APP_WiFi_Platform_GetFirmwareId());
      break;

    case APP_WIFI_STATE_CONSOLE_READY:
      APP_WiFi_Logf("[wifi] console: buf=0x%08lX size=%lu wr=%lu out=%lu\n",
                    (unsigned long)APP_WiFi_Platform_GetConsoleBufferAddress(),
                    (unsigned long)APP_WiFi_Platform_GetConsoleBufferSize(),
                    (unsigned long)APP_WiFi_Platform_GetConsoleWriteIndex(),
                    (unsigned long)APP_WiFi_Platform_GetConsoleOutIndex());
      break;

    case APP_WIFI_STATE_MAILBOX_READY:
      APP_WiFi_Logf("[wifi] mailbox: INT=0x%08lX data=0x%08lX SDIOirq=%lu OOB=%lu\n",
                    (unsigned long)APP_WiFi_Platform_GetInterruptStatus(),
                    (unsigned long)APP_WiFi_Platform_GetHostMailboxData(),
                    (unsigned long)APP_WiFi_Platform_GetSdioInterruptCount(),
                    (unsigned long)APP_WiFi_GetOobInterruptCount());
      break;

    case APP_WIFI_STATE_READY:
      APP_WiFi_Logf("[wifi] bus-up: clock=0x%02X IORDY=0x%02X hostMask=0x%08lX fnMask=0x%08lX\n",
                    (unsigned int)APP_WiFi_Platform_GetChipClockCsr(),
                    (unsigned int)APP_WiFi_Platform_GetCccrIoReady(),
                    (unsigned long)APP_WiFi_Platform_GetHostInterruptMask(),
                    (unsigned long)APP_WiFi_Platform_GetFunctionInterruptMask());
      break;

    case APP_WIFI_STATE_ERROR:
      APP_WiFi_Logf("[wifi] error: lastStatus=0x%08lX lastError=0x%08lX clock=0x%02X\n",
                    (unsigned long)APP_WiFi_Platform_GetLastSdioStatus(),
                    (unsigned long)APP_WiFi_Platform_GetLastSdioError(),
                    (unsigned int)APP_WiFi_Platform_GetChipClockCsr());
      break;

    default:
      break;
  }
}

static void APP_WiFi_LogPeriodicHeartbeat(void)
{
  const uint32_t now = HAL_GetTick();

  if ((now - g_wifiLastHeartbeatTick) < APP_WIFI_HEARTBEAT_MS)
  {
    return;
  }

  g_wifiLastHeartbeatTick = now;

  if (g_wifiState == APP_WIFI_STATE_MAILBOX_READY)
  {
    (void)APP_WiFi_Platform_ProbeConsole();
    (void)APP_WiFi_Platform_ProbeMailbox();

    APP_WiFi_Logf("[wifi] heartbeat: state=%s sdioIrq=%lu oob=%lu mailbox=0x%08lX int=0x%08lX consoleWr=%lu consoleOut=%lu\n",
                  APP_WiFi_StateToString(g_wifiState),
                  (unsigned long)APP_WiFi_Platform_GetSdioInterruptCount(),
                  (unsigned long)APP_WiFi_GetOobInterruptCount(),
                  (unsigned long)APP_WiFi_Platform_GetHostMailboxData(),
                  (unsigned long)APP_WiFi_Platform_GetInterruptStatus(),
                  (unsigned long)APP_WiFi_Platform_GetConsoleWriteIndex(),
                  (unsigned long)APP_WiFi_Platform_GetConsoleOutIndex());
    return;
  }

  if (g_wifiState == APP_WIFI_STATE_ERROR)
  {
    APP_WiFi_Logf("[wifi] heartbeat: state=%s lastStatus=0x%08lX lastError=0x%08lX sdioIrq=%lu oob=%lu\n",
                  APP_WiFi_StateToString(g_wifiState),
                  (unsigned long)APP_WiFi_Platform_GetLastSdioStatus(),
                  (unsigned long)APP_WiFi_Platform_GetLastSdioError(),
                  (unsigned long)APP_WiFi_Platform_GetSdioInterruptCount(),
                  (unsigned long)APP_WiFi_GetOobInterruptCount());
    return;
  }

  APP_WiFi_Logf("[wifi] heartbeat: state=%s sdioIrq=%lu oob=%lu\n",
                APP_WiFi_StateToString(g_wifiState),
                (unsigned long)APP_WiFi_Platform_GetSdioInterruptCount(),
                (unsigned long)APP_WiFi_GetOobInterruptCount());
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
          APP_WiFi_Logf("[wifi] pre-download setup failed: INTEN=0x%02X SEP=0x%02X IORDY=0x%02X\n",
                        (unsigned int)APP_WiFi_Platform_GetCccrInterruptEnable(),
                        (unsigned int)APP_WiFi_Platform_GetSepInterruptControl(),
                        (unsigned int)APP_WiFi_Platform_GetCccrIoReady());
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
          APP_WiFi_Logf("[wifi] pre-download setup failed: INTEN=0x%02X SEP=0x%02X IORDY=0x%02X\n",
                        (unsigned int)APP_WiFi_Platform_GetCccrInterruptEnable(),
                        (unsigned int)APP_WiFi_Platform_GetSepInterruptControl(),
                        (unsigned int)APP_WiFi_Platform_GetCccrIoReady());
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
        if (APP_WiFi_Platform_RunBackplaneWriteSmokeTest() == HAL_OK)
        {
          /*
           * The backplane write path now works too. From here we pivot to the
           * vendor-aligned pre-download sequence: enable Function 2 first, then
           * program the CCCR/OOB interrupt path, then start firmware staging.
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

      case APP_WIFI_STATE_HT_CLOCK_READY:
        if ((APP_WiFi_Platform_ConfigurePostFirmwareBus() == HAL_OK) &&
            (APP_WiFi_Platform_WaitForFunction2Ready() == HAL_OK))
        {
          /*
           * This is the closest checkpoint to the official 43362 SDIO bring-up:
           * firmware has booted, HT is available, the post-download backplane
           * interrupt masks/watermark are programmed, and Function 2 finally
           * reports ready. From here on, shared/console/mailbox probing is
           * treated as follow-on diagnostics rather than the bring-up gate.
           */
          APP_WiFi_SetState(APP_WIFI_STATE_READY);
          osDelay(APP_WIFI_STACK_WAIT_MS);
        }
        else
        {
          APP_WiFi_Logf("[wifi] post-fw bus setup failed: IORDY=0x%02X clock=0x%02X hostMask=0x%08lX fnMask=0x%08lX\n",
                        (unsigned int)APP_WiFi_Platform_GetCccrIoReady(),
                        (unsigned int)APP_WiFi_Platform_GetChipClockCsr(),
                        (unsigned long)APP_WiFi_Platform_GetHostInterruptMask(),
                        (unsigned long)APP_WiFi_Platform_GetFunctionInterruptMask());
          APP_WiFi_SetState(APP_WIFI_STATE_ERROR);
          osDelay(APP_WIFI_POLL_MS);
        }
        break;

      case APP_WIFI_STATE_FUNCTION2_READY:
        if (APP_WiFi_Platform_EnableFunction2() == HAL_OK)
        {
          /*
           * Function 2 is enabled in IOEN before firmware download, matching
           * the vendor SDIO bring-up sequence.
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
        if ((APP_WiFi_Platform_ConfigureInterruptPath() == HAL_OK) &&
            (APP_WiFi_Platform_PrepareFirmwareDownload() == HAL_OK) &&
            (APP_WiFi_Platform_ProbeFirmwareResources() == HAL_OK))
        {
          /*
           * This keeps the vendor-style pre-download setup together: pull-ups
           * off, separate OOB control programmed, CCCR INTEN set for master +
           * Function 2, the download-side core prep is complete, and the
           * firmware/NVRAM resource layout is computed.
           */
          APP_WiFi_SetState(APP_WIFI_STATE_RESOURCES_READY);
          osDelay(APP_WIFI_STACK_WAIT_MS);
        }
        else
        {
          APP_WiFi_Logf("[wifi] pre-download setup failed: INTEN=0x%02X SEP=0x%02X IORDY=0x%02X\n",
                        (unsigned int)APP_WiFi_Platform_GetCccrInterruptEnable(),
                        (unsigned int)APP_WiFi_Platform_GetSepInterruptControl(),
                        (unsigned int)APP_WiFi_Platform_GetCccrIoReady());
          APP_WiFi_SetState(APP_WIFI_STATE_ERROR);
          osDelay(APP_WIFI_POLL_MS);
        }
        break;

      case APP_WIFI_STATE_RESOURCES_READY:
        if (APP_WiFi_Platform_StageFirmwareImage() == HAL_OK)
        {
          /*
           * The firmware blob has now been copied into WLAN RAM. We still
           * stage NVRAM separately so download failures remain easy to isolate.
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
           * NVRAM and its trailer are now in place at the top of WLAN RAM.
           * The next stage can release the WLAN ARM core.
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
           * same minimal sequence as the vendor flow.
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
           * This is still the lightweight wrapper-based boot check: the WLAN
           * ARM core reports clock enabled and not in reset.
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
        if (APP_WiFi_Platform_RequestHtClock() == HAL_OK)
        {
          /*
           * This matches the next vendor checkpoint after firmware download:
           * request HT and wait for HT_AVAIL before treating the SDIO/WLAN bus
           * as really up.
           */
          APP_WiFi_SetState(APP_WIFI_STATE_HT_CLOCK_READY);
          osDelay(APP_WIFI_STACK_WAIT_MS);
        }
        else
        {
          APP_WiFi_Logf("[wifi] fw boot HT wait failed: clock=0x%02X IORDY=0x%02X\n",
                        (unsigned int)APP_WiFi_Platform_GetChipClockCsr(),
                        (unsigned int)APP_WiFi_Platform_GetCccrIoReady());
          APP_WiFi_SetState(APP_WIFI_STATE_ERROR);
          osDelay(APP_WIFI_POLL_MS);
        }
        break;

      case APP_WIFI_STATE_READY:
        if (APP_WiFi_Platform_ProbeSharedMemory() == HAL_OK)
        {
          APP_WiFi_SetState(APP_WIFI_STATE_SHARED_READY);
          osDelay(APP_WIFI_STACK_WAIT_MS);
        }
        else
        {
          APP_WiFi_LogPeriodicHeartbeat();
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
          APP_WiFi_Logf("[wifi] console probe pending: console=0x%08lX buf=0x%08lX size=%lu wr=%lu out=%lu\n",
                        (unsigned long)APP_WiFi_Platform_GetConsoleAddress(),
                        (unsigned long)APP_WiFi_Platform_GetConsoleBufferAddress(),
                        (unsigned long)APP_WiFi_Platform_GetConsoleBufferSize(),
                        (unsigned long)APP_WiFi_Platform_GetConsoleWriteIndex(),
                        (unsigned long)APP_WiFi_Platform_GetConsoleOutIndex());
          APP_WiFi_LogPeriodicHeartbeat();
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
          APP_WiFi_LogPeriodicHeartbeat();
          osDelay(APP_WIFI_POLL_MS);
        }
        break;

      case APP_WIFI_STATE_MAILBOX_READY:
        APP_WiFi_LogPeriodicHeartbeat();
        break;

      case APP_WIFI_STATE_ERROR:
        APP_WiFi_LogPeriodicHeartbeat();
      default:
        osDelay(APP_WIFI_POLL_MS);
        break;
    }
  }
}
