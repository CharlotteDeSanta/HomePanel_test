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
#define APP_WIFI_LOG_BUFFER_SIZE        512U
#define APP_WIFI_SDIO_FN2               2U
#define APP_WIFI_SDPCM_FRAME_AVAILABLE_MASK 0x000000F0U
#define APP_WIFI_SDPCM_HW_TAG_SIZE      4U
#define APP_WIFI_SDPCM_HEADER_SIZE      12U
#define APP_WIFI_SDPCM_CDC_HEADER_SIZE  16U
#define APP_WIFI_SDPCM_CONTROL_HEADER_CAPTURE_SIZE 64U
#define APP_WIFI_SDPCM_MAX_FRAME_LEN    2048U
#define APP_WIFI_SDPCM_CHANNEL_MASK     0x0FU
#define APP_WIFI_SDPCM_CHANNEL_CONTROL  0U
#define APP_WIFI_SDPCM_CHANNEL_EVENT    1U
#define APP_WIFI_SDPCM_CHANNEL_DATA     2U
#define APP_WIFI_SDPCM_IOCTL_GET        0x00U
#define APP_WIFI_SDPCM_IOCTL_SET        0x02U
#define APP_WIFI_SDPCM_IOCTL_ID_SHIFT   16U
#define APP_WIFI_SDPCM_IOCTL_ID_MASK    0xFFFF0000UL
#define APP_WIFI_SDPCM_IOCTL_IF_SHIFT   12U
#define APP_WIFI_IOCTL_U32_SIZE         4U
#define APP_WIFI_MAC_ADDRESS_SIZE       6U
#define APP_WIFI_WLC_GET_VERSION        1UL
#define APP_WIFI_WLC_UP                 2UL
#define APP_WIFI_WLC_SET_PM             86UL
#define APP_WIFI_WLC_GET_VAR            262UL
#define APP_WIFI_WLC_SET_VAR            263UL
#define APP_WIFI_IOVAR_CUR_ETHERADDR    "cur_etheraddr"
#define APP_WIFI_IOVAR_EVENT_MSGS       "event_msgs"
#define APP_WIFI_PM_OFF                 0UL
#define APP_WIFI_WL_EVENTING_MASK_LEN   19U

static volatile APP_WiFiState_t g_wifiState = APP_WIFI_STATE_IDLE;
static volatile uint32_t g_wifiOobInterruptCount = 0U;
static uint32_t g_wifiLastHeartbeatTick = 0U;
static uint32_t g_wifiSdpcmFrameCount = 0U;
static uint32_t g_wifiLastSdpcmFrameInterrupt = 0U;
static uint16_t g_wifiLastSdpcmFrameLength = 0U;
static uint8_t g_wifiLastSdpcmSequence = 0U;
static uint8_t g_wifiLastSdpcmChannel = 0U;
static uint8_t g_wifiLastSdpcmHeaderLength = 0U;
static uint8_t g_wifiLastSdpcmNextLength = 0U;
static uint8_t g_wifiLastSdpcmBusCredit = 0U;
static uint8_t g_wifiLastSdpcmFlowControl = 0U;
static uint8_t g_wifiBusCredit = 1U;
static uint8_t g_wifiBusCreditDiff = 0U;
static uint8_t g_wifiTxSequence = 0U;
static uint16_t g_wifiIoctlRequestId = 0U;
static uint8_t g_wifiIoctlProbeSent = 0U;
static uint8_t g_wifiIoctlProbeCompleted = 0U;
static uint8_t g_wifiVersionProbeCompleted = 0U;
static uint8_t g_wifiUpProbeSent = 0U;
static uint8_t g_wifiUpProbeCompleted = 0U;
static uint8_t g_wifiMacProbeSent = 0U;
static uint8_t g_wifiMacProbeCompleted = 0U;
static uint8_t g_wifiEventMaskSent = 0U;
static uint8_t g_wifiEventMaskCompleted = 0U;
static uint8_t g_wifiPmProbeSent = 0U;
static uint8_t g_wifiPmProbeCompleted = 0U;
static uint8_t g_wifiLastMacAddress[APP_WIFI_MAC_ADDRESS_SIZE] = {0U};
static uint32_t g_wifiLastIoctlCommand = 0U;
static uint32_t g_wifiLastIoctlStatus = 0U;
static uint32_t g_wifiLastIoctlFlags = 0U;
static uint32_t g_wifiLastIoctlValue = 0U;

static const char *APP_WiFi_StateToString(APP_WiFiState_t state);
static const char *APP_WiFi_SdpcmChannelToString(uint8_t channel);
static void APP_WiFi_Logf(const char *format, ...);
static void APP_WiFi_LogStateDetails(APP_WiFiState_t state);
static void APP_WiFi_LogPeriodicHeartbeat(void);
static uint16_t APP_WiFi_ReadLe16(const uint8_t *bytes);
static uint32_t APP_WiFi_ReadLe32(const uint8_t *bytes);
static void APP_WiFi_WriteLe16(uint8_t *bytes, uint16_t value);
static void APP_WiFi_WriteLe32(uint8_t *bytes, uint32_t value);
static uint16_t APP_WiFi_GetSdpcmChunkSize(uint16_t remaining);
static void APP_WiFi_RecordSdpcmHeader(const uint8_t *header, uint16_t frameLength, uint32_t frameInterrupt);
static void APP_WiFi_UpdateSdpcmCredit(const uint8_t *header);
static void APP_WiFi_LogSdpcmBytes(const char *prefix, const uint8_t *data, uint16_t length);
static void APP_WiFi_LogMacAddress(const char *prefix, const uint8_t *macAddress);
static void APP_WiFi_SetEventMaskBit(uint8_t *eventMask, uint16_t eventNumber);
static HAL_StatusTypeDef APP_WiFi_SendControlIoctl(uint8_t ioctlType,
                                                   uint32_t command,
                                                   const void *payload,
                                                   uint16_t payloadLength,
                                                   const char *name);
static HAL_StatusTypeDef APP_WiFi_SendGetVersionIoctl(void);
static HAL_StatusTypeDef APP_WiFi_SendUpIoctl(void);
static HAL_StatusTypeDef APP_WiFi_SendGetCurEtheraddrIovar(void);
static HAL_StatusTypeDef APP_WiFi_SendSetEventMsgsIovar(void);
static HAL_StatusTypeDef APP_WiFi_SendSetPmOffIoctl(void);
static uint8_t APP_WiFi_TryProbeSdpcmRx(void);

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
  g_wifiSdpcmFrameCount = 0U;
  g_wifiLastSdpcmFrameInterrupt = 0U;
  g_wifiLastSdpcmFrameLength = 0U;
  g_wifiLastSdpcmSequence = 0U;
  g_wifiLastSdpcmChannel = 0U;
  g_wifiLastSdpcmHeaderLength = 0U;
  g_wifiLastSdpcmNextLength = 0U;
  g_wifiLastSdpcmBusCredit = 0U;
  g_wifiLastSdpcmFlowControl = 0U;
  g_wifiBusCredit = 1U;
  g_wifiBusCreditDiff = 0U;
  g_wifiTxSequence = 0U;
  g_wifiIoctlRequestId = 0U;
  g_wifiIoctlProbeSent = 0U;
  g_wifiIoctlProbeCompleted = 0U;
  g_wifiVersionProbeCompleted = 0U;
  g_wifiUpProbeSent = 0U;
  g_wifiUpProbeCompleted = 0U;
  g_wifiMacProbeSent = 0U;
  g_wifiMacProbeCompleted = 0U;
  g_wifiEventMaskSent = 0U;
  g_wifiEventMaskCompleted = 0U;
  g_wifiPmProbeSent = 0U;
  g_wifiPmProbeCompleted = 0U;
  memset(g_wifiLastMacAddress, 0, sizeof(g_wifiLastMacAddress));
  g_wifiLastIoctlCommand = 0U;
  g_wifiLastIoctlStatus = 0U;
  g_wifiLastIoctlFlags = 0U;
  g_wifiLastIoctlValue = 0U;
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

static const char *APP_WiFi_SdpcmChannelToString(uint8_t channel)
{
  switch (channel)
  {
    case APP_WIFI_SDPCM_CHANNEL_CONTROL:
      return "control";
    case APP_WIFI_SDPCM_CHANNEL_EVENT:
      return "event";
    case APP_WIFI_SDPCM_CHANNEL_DATA:
      return "data";
    default:
      return "other";
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

static uint16_t APP_WiFi_ReadLe16(const uint8_t *bytes)
{
  if (bytes == NULL)
  {
    return 0U;
  }

  return (uint16_t)(((uint16_t)bytes[0]) | ((uint16_t)bytes[1] << 8U));
}

static uint32_t APP_WiFi_ReadLe32(const uint8_t *bytes)
{
  if (bytes == NULL)
  {
    return 0U;
  }

  return ((uint32_t)bytes[0]) |
         ((uint32_t)bytes[1] << 8U) |
         ((uint32_t)bytes[2] << 16U) |
         ((uint32_t)bytes[3] << 24U);
}

static void APP_WiFi_WriteLe16(uint8_t *bytes, uint16_t value)
{
  if (bytes == NULL)
  {
    return;
  }

  bytes[0] = (uint8_t)(value & 0xFFU);
  bytes[1] = (uint8_t)((value >> 8U) & 0xFFU);
}

static void APP_WiFi_WriteLe32(uint8_t *bytes, uint32_t value)
{
  if (bytes == NULL)
  {
    return;
  }

  bytes[0] = (uint8_t)(value & 0xFFU);
  bytes[1] = (uint8_t)((value >> 8U) & 0xFFU);
  bytes[2] = (uint8_t)((value >> 16U) & 0xFFU);
  bytes[3] = (uint8_t)((value >> 24U) & 0xFFU);
}

static uint16_t APP_WiFi_GetSdpcmChunkSize(uint16_t remaining)
{
  return (remaining > 4U) ? 4U : remaining;
}

static void APP_WiFi_RecordSdpcmHeader(const uint8_t *header, uint16_t frameLength, uint32_t frameInterrupt)
{
  if (header == NULL)
  {
    return;
  }

  g_wifiSdpcmFrameCount++;
  g_wifiLastSdpcmFrameInterrupt = frameInterrupt;
  g_wifiLastSdpcmFrameLength = frameLength;
  g_wifiLastSdpcmSequence = header[4];
  g_wifiLastSdpcmChannel = (uint8_t)(header[5] & APP_WIFI_SDPCM_CHANNEL_MASK);
  g_wifiLastSdpcmNextLength = header[6];
  g_wifiLastSdpcmHeaderLength = header[7];
  g_wifiLastSdpcmFlowControl = header[8];
  g_wifiLastSdpcmBusCredit = header[9];

  APP_WiFi_Logf("[wifi] sdpcm: #%lu irq=0x%08lX len=%u seq=%u ch=%s(%u) hdr=%u next=%u credit=%u flow=%u\n",
                (unsigned long)g_wifiSdpcmFrameCount,
                (unsigned long)frameInterrupt,
                (unsigned int)frameLength,
                (unsigned int)g_wifiLastSdpcmSequence,
                APP_WiFi_SdpcmChannelToString(g_wifiLastSdpcmChannel),
                (unsigned int)g_wifiLastSdpcmChannel,
                (unsigned int)g_wifiLastSdpcmHeaderLength,
                (unsigned int)g_wifiLastSdpcmNextLength,
                (unsigned int)g_wifiLastSdpcmBusCredit,
                (unsigned int)g_wifiLastSdpcmFlowControl);
}

static void APP_WiFi_UpdateSdpcmCredit(const uint8_t *header)
{
  uint8_t creditDiff = 0U;

  if (header == NULL)
  {
    return;
  }

  if ((header[5] & APP_WIFI_SDPCM_CHANNEL_MASK) >= 3U)
  {
    return;
  }

  creditDiff = (uint8_t)(header[9] - g_wifiBusCredit);
  if (creditDiff <= 15U)
  {
    g_wifiBusCredit = header[9];
    g_wifiBusCreditDiff = creditDiff;
  }
}

static void APP_WiFi_LogSdpcmBytes(const char *prefix, const uint8_t *data, uint16_t length)
{
  char logBuffer[APP_WIFI_LOG_BUFFER_SIZE];
  int written = 0;
  uint16_t index = 0U;

  if ((prefix == NULL) || (data == NULL) || (length == 0U))
  {
    return;
  }

  written = snprintf(logBuffer, sizeof(logBuffer), "%s", prefix);
  if ((written <= 0) || ((size_t)written >= sizeof(logBuffer)))
  {
    return;
  }

  for (index = 0U; index < length; ++index)
  {
    const int chunkWritten = snprintf(&logBuffer[written],
                                      sizeof(logBuffer) - (size_t)written,
                                      "%s%02X",
                                      (index == 0U) ? "" : " ",
                                      (unsigned int)data[index]);
    if ((chunkWritten <= 0) || ((size_t)chunkWritten >= (sizeof(logBuffer) - (size_t)written)))
    {
      break;
    }

    written += chunkWritten;
  }

  if ((size_t)written < (sizeof(logBuffer) - 2U))
  {
    logBuffer[written++] = '\n';
    logBuffer[written] = '\0';
  }
  else
  {
    logBuffer[sizeof(logBuffer) - 2U] = '\n';
    logBuffer[sizeof(logBuffer) - 1U] = '\0';
  }

  (void)APP_DebugUart_WriteString(logBuffer);
}

static void APP_WiFi_LogMacAddress(const char *prefix, const uint8_t *macAddress)
{
  if ((prefix == NULL) || (macAddress == NULL))
  {
    return;
  }

  APP_WiFi_Logf("%s%02X:%02X:%02X:%02X:%02X:%02X\n",
                prefix,
                (unsigned int)macAddress[0],
                (unsigned int)macAddress[1],
                (unsigned int)macAddress[2],
                (unsigned int)macAddress[3],
                (unsigned int)macAddress[4],
                (unsigned int)macAddress[5]);
}

static void APP_WiFi_SetEventMaskBit(uint8_t *eventMask, uint16_t eventNumber)
{
  if ((eventMask == NULL) || (eventNumber >= (APP_WIFI_WL_EVENTING_MASK_LEN * 8U)))
  {
    return;
  }

  eventMask[eventNumber / 8U] |= (uint8_t)(1U << (eventNumber % 8U));
}

static HAL_StatusTypeDef APP_WiFi_SendControlIoctl(uint8_t ioctlType,
                                                   uint32_t command,
                                                   const void *payload,
                                                   uint16_t payloadLength,
                                                   const char *name)
{
  uint8_t frame[APP_WIFI_SDPCM_HEADER_SIZE + APP_WIFI_SDPCM_CDC_HEADER_SIZE + APP_WIFI_IOCTL_U32_SIZE] = {0};
  uint32_t flags = 0U;
  uint8_t availableCredits = 0U;
  uint8_t txSequence = 0U;
  uint16_t ioctlRequestId = 0U;
  const uint16_t frameLength = (uint16_t)(APP_WIFI_SDPCM_HEADER_SIZE + APP_WIFI_SDPCM_CDC_HEADER_SIZE + payloadLength);

  if (payloadLength > APP_WIFI_IOCTL_U32_SIZE)
  {
    return HAL_ERROR;
  }

  availableCredits = (uint8_t)(g_wifiBusCredit - g_wifiTxSequence);
  if (availableCredits == 0U)
  {
    APP_WiFi_Logf("[wifi] ioctl: no credits available for %s tx=%u credit=%u\n",
                  name,
                  (unsigned int)g_wifiTxSequence,
                  (unsigned int)g_wifiBusCredit);
    return HAL_BUSY;
  }

  APP_WiFi_WriteLe16(&frame[0], frameLength);
  APP_WiFi_WriteLe16(&frame[2], (uint16_t)(~frameLength));
  txSequence = g_wifiTxSequence;
  frame[4] = txSequence;
  frame[5] = APP_WIFI_SDPCM_CHANNEL_CONTROL;
  frame[7] = APP_WIFI_SDPCM_HEADER_SIZE;

  APP_WiFi_WriteLe32(&frame[APP_WIFI_SDPCM_HEADER_SIZE + 0U], command);
  APP_WiFi_WriteLe32(&frame[APP_WIFI_SDPCM_HEADER_SIZE + 4U], payloadLength);

  ioctlRequestId = (uint16_t)(g_wifiIoctlRequestId + 1U);
  flags = ((((uint32_t)ioctlRequestId << APP_WIFI_SDPCM_IOCTL_ID_SHIFT) & APP_WIFI_SDPCM_IOCTL_ID_MASK) |
           (((uint32_t)0U) << APP_WIFI_SDPCM_IOCTL_IF_SHIFT) |
           ioctlType);
  APP_WiFi_WriteLe32(&frame[APP_WIFI_SDPCM_HEADER_SIZE + 8U], flags);
  APP_WiFi_WriteLe32(&frame[APP_WIFI_SDPCM_HEADER_SIZE + 12U], 0U);

  if ((payload != NULL) && (payloadLength != 0U))
  {
    memcpy(&frame[APP_WIFI_SDPCM_HEADER_SIZE + APP_WIFI_SDPCM_CDC_HEADER_SIZE], payload, payloadLength);
  }

  if (APP_WiFi_Platform_Fn2Write(frame, frameLength) != HAL_OK)
  {
    APP_WiFi_Logf("[wifi] ioctl: send %s failed id=%u sta=0x%08lX err=0x%08lX\n",
                  name,
                  (unsigned int)ioctlRequestId,
                  (unsigned long)APP_WiFi_Platform_GetLastSdioStatus(),
                  (unsigned long)APP_WiFi_Platform_GetLastSdioError());
    return HAL_ERROR;
  }

  g_wifiTxSequence = (uint8_t)(txSequence + 1U);
  g_wifiIoctlRequestId = ioctlRequestId;
  g_wifiIoctlProbeSent = 1U;
  g_wifiIoctlProbeCompleted = 0U;
  g_wifiLastIoctlCommand = command;
  g_wifiLastIoctlFlags = flags;
  g_wifiLastIoctlStatus = 0U;
  g_wifiLastIoctlValue = 0U;
  APP_WiFi_Logf("[wifi] ioctl: sent %s id=%u txseq=%u credits=%u len=%u type=%u\n",
                name,
                (unsigned int)g_wifiIoctlRequestId,
                (unsigned int)txSequence,
                (unsigned int)availableCredits,
                (unsigned int)payloadLength,
                (unsigned int)ioctlType);
  return HAL_OK;
}

static HAL_StatusTypeDef APP_WiFi_SendGetVersionIoctl(void)
{
  const uint32_t versionPlaceholder = 0U;

  return APP_WiFi_SendControlIoctl(APP_WIFI_SDPCM_IOCTL_GET,
                                   APP_WIFI_WLC_GET_VERSION,
                                   &versionPlaceholder,
                                   APP_WIFI_IOCTL_U32_SIZE,
                                   "WLC_GET_VERSION");
}

static HAL_StatusTypeDef APP_WiFi_SendUpIoctl(void)
{
  return APP_WiFi_SendControlIoctl(APP_WIFI_SDPCM_IOCTL_SET,
                                   APP_WIFI_WLC_UP,
                                   NULL,
                                   0U,
                                   "WLC_UP");
}

static HAL_StatusTypeDef APP_WiFi_SendGetCurEtheraddrIovar(void)
{
  uint8_t frame[APP_WIFI_SDPCM_HEADER_SIZE + APP_WIFI_SDPCM_CDC_HEADER_SIZE + 32U] = {0};
  const char *iovarName = APP_WIFI_IOVAR_CUR_ETHERADDR;
  const uint16_t nameLength = (uint16_t)(strlen(iovarName) + 1U);
  const uint16_t payloadLength = (uint16_t)(nameLength + APP_WIFI_MAC_ADDRESS_SIZE);
  const uint16_t frameLength = (uint16_t)(APP_WIFI_SDPCM_HEADER_SIZE + APP_WIFI_SDPCM_CDC_HEADER_SIZE + payloadLength);
  uint32_t flags = 0U;
  uint8_t availableCredits = 0U;
  uint8_t txSequence = 0U;
  uint16_t ioctlRequestId = 0U;

  availableCredits = (uint8_t)(g_wifiBusCredit - g_wifiTxSequence);
  if (availableCredits == 0U)
  {
    APP_WiFi_Logf("[wifi] iovar: no credits available for %s tx=%u credit=%u\n",
                  APP_WIFI_IOVAR_CUR_ETHERADDR,
                  (unsigned int)g_wifiTxSequence,
                  (unsigned int)g_wifiBusCredit);
    return HAL_BUSY;
  }

  APP_WiFi_WriteLe16(&frame[0], frameLength);
  APP_WiFi_WriteLe16(&frame[2], (uint16_t)(~frameLength));
  txSequence = g_wifiTxSequence;
  frame[4] = txSequence;
  frame[5] = APP_WIFI_SDPCM_CHANNEL_CONTROL;
  frame[7] = APP_WIFI_SDPCM_HEADER_SIZE;

  APP_WiFi_WriteLe32(&frame[APP_WIFI_SDPCM_HEADER_SIZE + 0U], APP_WIFI_WLC_GET_VAR);
  APP_WiFi_WriteLe32(&frame[APP_WIFI_SDPCM_HEADER_SIZE + 4U], payloadLength);

  ioctlRequestId = (uint16_t)(g_wifiIoctlRequestId + 1U);
  flags = ((((uint32_t)ioctlRequestId << APP_WIFI_SDPCM_IOCTL_ID_SHIFT) & APP_WIFI_SDPCM_IOCTL_ID_MASK) |
           (((uint32_t)0U) << APP_WIFI_SDPCM_IOCTL_IF_SHIFT) |
           APP_WIFI_SDPCM_IOCTL_GET);
  APP_WiFi_WriteLe32(&frame[APP_WIFI_SDPCM_HEADER_SIZE + 8U], flags);
  APP_WiFi_WriteLe32(&frame[APP_WIFI_SDPCM_HEADER_SIZE + 12U], 0U);

  memcpy(&frame[APP_WIFI_SDPCM_HEADER_SIZE + APP_WIFI_SDPCM_CDC_HEADER_SIZE],
         iovarName,
         nameLength);

  if (APP_WiFi_Platform_Fn2Write(frame, frameLength) != HAL_OK)
  {
    APP_WiFi_Logf("[wifi] iovar: send %s failed id=%u sta=0x%08lX err=0x%08lX\n",
                  APP_WIFI_IOVAR_CUR_ETHERADDR,
                  (unsigned int)ioctlRequestId,
                  (unsigned long)APP_WiFi_Platform_GetLastSdioStatus(),
                  (unsigned long)APP_WiFi_Platform_GetLastSdioError());
    return HAL_ERROR;
  }

  g_wifiTxSequence = (uint8_t)(txSequence + 1U);
  g_wifiIoctlRequestId = ioctlRequestId;
  g_wifiIoctlProbeSent = 1U;
  g_wifiIoctlProbeCompleted = 0U;
  g_wifiLastIoctlCommand = APP_WIFI_WLC_GET_VAR;
  g_wifiLastIoctlFlags = flags;
  g_wifiLastIoctlStatus = 0U;
  g_wifiLastIoctlValue = 0U;
  APP_WiFi_Logf("[wifi] iovar: sent %s id=%u txseq=%u credits=%u len=%u\n",
                APP_WIFI_IOVAR_CUR_ETHERADDR,
                (unsigned int)g_wifiIoctlRequestId,
                (unsigned int)txSequence,
                (unsigned int)availableCredits,
                (unsigned int)APP_WIFI_MAC_ADDRESS_SIZE);
  return HAL_OK;
}

static HAL_StatusTypeDef APP_WiFi_SendSetEventMsgsIovar(void)
{
  uint8_t frame[APP_WIFI_SDPCM_HEADER_SIZE + APP_WIFI_SDPCM_CDC_HEADER_SIZE + 64U] = {0};
  const char *iovarName = APP_WIFI_IOVAR_EVENT_MSGS;
  const uint16_t nameLength = (uint16_t)(strlen(iovarName) + 1U);
  const uint16_t payloadLength = (uint16_t)(nameLength + APP_WIFI_WL_EVENTING_MASK_LEN);
  const uint16_t frameLength = (uint16_t)(APP_WIFI_SDPCM_HEADER_SIZE + APP_WIFI_SDPCM_CDC_HEADER_SIZE + payloadLength);
  uint8_t *eventMask = &frame[APP_WIFI_SDPCM_HEADER_SIZE + APP_WIFI_SDPCM_CDC_HEADER_SIZE + nameLength];
  uint32_t flags = 0U;
  uint8_t availableCredits = 0U;
  uint8_t txSequence = 0U;
  uint16_t ioctlRequestId = 0U;

  availableCredits = (uint8_t)(g_wifiBusCredit - g_wifiTxSequence);
  if (availableCredits == 0U)
  {
    APP_WiFi_Logf("[wifi] iovar: no credits available for %s tx=%u credit=%u\n",
                  APP_WIFI_IOVAR_EVENT_MSGS,
                  (unsigned int)g_wifiTxSequence,
                  (unsigned int)g_wifiBusCredit);
    return HAL_BUSY;
  }

  APP_WiFi_WriteLe16(&frame[0], frameLength);
  APP_WiFi_WriteLe16(&frame[2], (uint16_t)(~frameLength));
  txSequence = g_wifiTxSequence;
  frame[4] = txSequence;
  frame[5] = APP_WIFI_SDPCM_CHANNEL_CONTROL;
  frame[7] = APP_WIFI_SDPCM_HEADER_SIZE;

  APP_WiFi_WriteLe32(&frame[APP_WIFI_SDPCM_HEADER_SIZE + 0U], APP_WIFI_WLC_SET_VAR);
  APP_WiFi_WriteLe32(&frame[APP_WIFI_SDPCM_HEADER_SIZE + 4U], payloadLength);

  ioctlRequestId = (uint16_t)(g_wifiIoctlRequestId + 1U);
  flags = ((((uint32_t)ioctlRequestId << APP_WIFI_SDPCM_IOCTL_ID_SHIFT) & APP_WIFI_SDPCM_IOCTL_ID_MASK) |
           (((uint32_t)0U) << APP_WIFI_SDPCM_IOCTL_IF_SHIFT) |
           APP_WIFI_SDPCM_IOCTL_SET);
  APP_WiFi_WriteLe32(&frame[APP_WIFI_SDPCM_HEADER_SIZE + 8U], flags);
  APP_WiFi_WriteLe32(&frame[APP_WIFI_SDPCM_HEADER_SIZE + 12U], 0U);

  memcpy(&frame[APP_WIFI_SDPCM_HEADER_SIZE + APP_WIFI_SDPCM_CDC_HEADER_SIZE],
         iovarName,
         nameLength);

  memset(eventMask, 0, APP_WIFI_WL_EVENTING_MASK_LEN);
  APP_WiFi_SetEventMaskBit(eventMask, 0U);   /* WLC_E_SET_SSID */
  APP_WiFi_SetEventMaskBit(eventMask, 1U);   /* WLC_E_JOIN */
  APP_WiFi_SetEventMaskBit(eventMask, 3U);   /* WLC_E_AUTH */
  APP_WiFi_SetEventMaskBit(eventMask, 5U);   /* WLC_E_DEAUTH */
  APP_WiFi_SetEventMaskBit(eventMask, 7U);   /* WLC_E_ASSOC */
  APP_WiFi_SetEventMaskBit(eventMask, 11U);  /* WLC_E_DISASSOC */
  APP_WiFi_SetEventMaskBit(eventMask, 16U);  /* WLC_E_LINK */
  APP_WiFi_SetEventMaskBit(eventMask, 23U);  /* WLC_E_PRUNE */
  APP_WiFi_SetEventMaskBit(eventMask, 26U);  /* WLC_E_SCAN_COMPLETE */
  APP_WiFi_SetEventMaskBit(eventMask, 46U);  /* WLC_E_PSK_SUP */
  APP_WiFi_SetEventMaskBit(eventMask, 69U);  /* WLC_E_ESCAN_RESULT */

  if (APP_WiFi_Platform_Fn2Write(frame, frameLength) != HAL_OK)
  {
    APP_WiFi_Logf("[wifi] iovar: send %s failed id=%u sta=0x%08lX err=0x%08lX\n",
                  APP_WIFI_IOVAR_EVENT_MSGS,
                  (unsigned int)ioctlRequestId,
                  (unsigned long)APP_WiFi_Platform_GetLastSdioStatus(),
                  (unsigned long)APP_WiFi_Platform_GetLastSdioError());
    return HAL_ERROR;
  }

  g_wifiTxSequence = (uint8_t)(txSequence + 1U);
  g_wifiIoctlRequestId = ioctlRequestId;
  g_wifiIoctlProbeSent = 1U;
  g_wifiIoctlProbeCompleted = 0U;
  g_wifiLastIoctlCommand = APP_WIFI_WLC_SET_VAR;
  g_wifiLastIoctlFlags = flags;
  g_wifiLastIoctlStatus = 0U;
  g_wifiLastIoctlValue = 0U;
  APP_WiFi_Logf("[wifi] iovar: sent %s id=%u txseq=%u credits=%u maskLen=%u\n",
                APP_WIFI_IOVAR_EVENT_MSGS,
                (unsigned int)g_wifiIoctlRequestId,
                (unsigned int)txSequence,
                (unsigned int)availableCredits,
                (unsigned int)APP_WIFI_WL_EVENTING_MASK_LEN);
  return HAL_OK;
}

static HAL_StatusTypeDef APP_WiFi_SendSetPmOffIoctl(void)
{
  const uint32_t pmValue = APP_WIFI_PM_OFF;

  return APP_WiFi_SendControlIoctl(APP_WIFI_SDPCM_IOCTL_SET,
                                   APP_WIFI_WLC_SET_PM,
                                   &pmValue,
                                   APP_WIFI_IOCTL_U32_SIZE,
                                   "WLC_SET_PM(PM_OFF)");
}

static uint8_t APP_WiFi_TryProbeSdpcmRx(void)
{
  uint32_t frameInterrupt = 0U;
  uint8_t header[APP_WIFI_SDPCM_CONTROL_HEADER_CAPTURE_SIZE] = {0};
  uint8_t chunkBuffer[64] = {0};
  uint8_t hwtagReadSize = APP_WIFI_SDPCM_HW_TAG_SIZE;
  uint16_t frameLength = 0U;
  uint16_t frameCheck = 0U;
  uint16_t remaining = 0U;
  uint16_t captured = APP_WIFI_SDPCM_HW_TAG_SIZE;

  if (APP_WiFi_Platform_GetFunction2RxInterruptStatus(&frameInterrupt) != HAL_OK)
  {
    return 0U;
  }

  if ((frameInterrupt & APP_WIFI_SDPCM_FRAME_AVAILABLE_MASK) == 0U)
  {
    return 0U;
  }

  if (APP_WiFi_Platform_Fn2Read(header, APP_WIFI_SDPCM_HW_TAG_SIZE) != HAL_OK)
  {
    (void)APP_WiFi_Platform_AbortFunction2Read();

    if (APP_WiFi_Platform_Fn2Read(chunkBuffer, (uint16_t)sizeof(chunkBuffer)) != HAL_OK)
    {
      APP_WiFi_Logf("[wifi] sdpcm: hwtag read failed irq=0x%08lX sta=0x%08lX err=0x%08lX\n",
                    (unsigned long)frameInterrupt,
                    (unsigned long)APP_WiFi_Platform_GetLastSdioStatus(),
                    (unsigned long)APP_WiFi_Platform_GetLastSdioError());
      (void)APP_WiFi_Platform_AbortFunction2Read();
      (void)APP_WiFi_Platform_ClearFunction2RxInterrupt(frameInterrupt);
      return 0U;
    }

    memcpy(header, chunkBuffer, APP_WIFI_SDPCM_HW_TAG_SIZE);
    hwtagReadSize = (uint8_t)sizeof(chunkBuffer);
    captured = (hwtagReadSize > APP_WIFI_SDPCM_CONTROL_HEADER_CAPTURE_SIZE) ? APP_WIFI_SDPCM_CONTROL_HEADER_CAPTURE_SIZE : hwtagReadSize;
    if (captured > APP_WIFI_SDPCM_HW_TAG_SIZE)
    {
      memcpy(&header[APP_WIFI_SDPCM_HW_TAG_SIZE],
             &chunkBuffer[APP_WIFI_SDPCM_HW_TAG_SIZE],
             (size_t)(captured - APP_WIFI_SDPCM_HW_TAG_SIZE));
    }

    APP_WiFi_Logf("[wifi] sdpcm: hwtag fallback used irq=0x%08lX\n",
                  (unsigned long)frameInterrupt);
  }

  frameLength = APP_WiFi_ReadLe16(&header[0]);
  frameCheck = APP_WiFi_ReadLe16(&header[2]);

  if ((frameLength == 0U) || ((uint16_t)(frameLength ^ frameCheck) != 0xFFFFU))
  {
    APP_WiFi_Logf("[wifi] sdpcm: hwtag mismatch irq=0x%08lX len=0x%04X chk=0x%04X\n",
                  (unsigned long)frameInterrupt,
                  (unsigned int)frameLength,
                  (unsigned int)frameCheck);
    APP_WiFi_LogSdpcmBytes("[wifi] sdpcm: raw=",
                           header,
                           captured);
    (void)APP_WiFi_Platform_AbortFunction2Read();
    (void)APP_WiFi_Platform_ClearFunction2RxInterrupt(frameInterrupt);
    return 0U;
  }

  if (frameLength > APP_WIFI_SDPCM_MAX_FRAME_LEN)
  {
    APP_WiFi_Logf("[wifi] sdpcm: frame too large irq=0x%08lX len=%u\n",
                  (unsigned long)frameInterrupt,
                  (unsigned int)frameLength);
    (void)APP_WiFi_Platform_AbortFunction2Read();
    (void)APP_WiFi_Platform_ClearFunction2RxInterrupt(frameInterrupt);
    return 0U;
  }

  remaining = (frameLength > hwtagReadSize) ? (uint16_t)(frameLength - hwtagReadSize) : 0U;
  while (remaining > 0U)
  {
    const uint16_t chunkSize = APP_WiFi_GetSdpcmChunkSize(remaining);
    uint16_t copyLength = 0U;
    HAL_StatusTypeDef status = HAL_ERROR;

    if (chunkSize == 1U)
    {
      status = APP_WiFi_Platform_Cmd52Read(APP_WIFI_SDIO_FN2, 0U, &chunkBuffer[0]);
    }
    else
    {
      status = APP_WiFi_Platform_Fn2Read(chunkBuffer, chunkSize);
    }

    if (status != HAL_OK)
    {
      APP_WiFi_Logf("[wifi] sdpcm: payload read failed irq=0x%08lX len=%u rem=%u sta=0x%08lX err=0x%08lX\n",
                    (unsigned long)frameInterrupt,
                    (unsigned int)frameLength,
                    (unsigned int)remaining,
                    (unsigned long)APP_WiFi_Platform_GetLastSdioStatus(),
                    (unsigned long)APP_WiFi_Platform_GetLastSdioError());
      (void)APP_WiFi_Platform_AbortFunction2Read();
      (void)APP_WiFi_Platform_ClearFunction2RxInterrupt(frameInterrupt);
      return 0U;
    }

    if (captured < APP_WIFI_SDPCM_CONTROL_HEADER_CAPTURE_SIZE)
    {
      copyLength = (uint16_t)(APP_WIFI_SDPCM_CONTROL_HEADER_CAPTURE_SIZE - captured);
      if (copyLength > chunkSize)
      {
        copyLength = chunkSize;
      }

      memcpy(&header[captured], chunkBuffer, copyLength);
      captured = (uint16_t)(captured + copyLength);
    }

    remaining = (uint16_t)(remaining - chunkSize);
  }

  if (frameLength < APP_WIFI_SDPCM_HEADER_SIZE)
  {
    APP_WiFi_Logf("[wifi] sdpcm: short frame irq=0x%08lX len=%u\n",
                  (unsigned long)frameInterrupt,
                  (unsigned int)frameLength);
    (void)APP_WiFi_Platform_ClearFunction2RxInterrupt(frameInterrupt);
    return 1U;
  }

  APP_WiFi_UpdateSdpcmCredit(header);
  APP_WiFi_RecordSdpcmHeader(header, frameLength, frameInterrupt);
  if ((g_wifiLastSdpcmChannel == APP_WIFI_SDPCM_CHANNEL_CONTROL) &&
      (captured >= (APP_WIFI_SDPCM_HEADER_SIZE + APP_WIFI_SDPCM_CDC_HEADER_SIZE)))
  {
    const uint32_t command = APP_WiFi_ReadLe32(&header[APP_WIFI_SDPCM_HEADER_SIZE + 0U]);
    const uint32_t payloadLength = APP_WiFi_ReadLe32(&header[APP_WIFI_SDPCM_HEADER_SIZE + 4U]);
    const uint32_t flags = APP_WiFi_ReadLe32(&header[APP_WIFI_SDPCM_HEADER_SIZE + 8U]);
    const uint32_t status = APP_WiFi_ReadLe32(&header[APP_WIFI_SDPCM_HEADER_SIZE + 12U]);

    g_wifiLastIoctlCommand = command;
    g_wifiLastIoctlFlags = flags;
    g_wifiLastIoctlStatus = status;

    if ((command == APP_WIFI_WLC_GET_VERSION) &&
        (payloadLength >= sizeof(uint32_t)) &&
        (captured >= (APP_WIFI_SDPCM_HEADER_SIZE + APP_WIFI_SDPCM_CDC_HEADER_SIZE + sizeof(uint32_t))))
    {
      g_wifiLastIoctlValue = APP_WiFi_ReadLe32(&header[APP_WIFI_SDPCM_HEADER_SIZE + APP_WIFI_SDPCM_CDC_HEADER_SIZE]);
    }

    if ((command == APP_WIFI_WLC_GET_VAR) &&
        (payloadLength >= APP_WIFI_MAC_ADDRESS_SIZE) &&
        (captured >= (APP_WIFI_SDPCM_HEADER_SIZE + APP_WIFI_SDPCM_CDC_HEADER_SIZE + APP_WIFI_MAC_ADDRESS_SIZE)))
    {
      memcpy(g_wifiLastMacAddress,
             &header[APP_WIFI_SDPCM_HEADER_SIZE + APP_WIFI_SDPCM_CDC_HEADER_SIZE],
             APP_WIFI_MAC_ADDRESS_SIZE);
      APP_WiFi_LogMacAddress("[wifi] iovar: rsp cur_etheraddr mac=", g_wifiLastMacAddress);
    }

    APP_WiFi_Logf("[wifi] ioctl: rsp cmd=%lu id=%u len=%lu status=0x%08lX value=0x%08lX\n",
                  (unsigned long)command,
                  (unsigned int)((flags & APP_WIFI_SDPCM_IOCTL_ID_MASK) >> APP_WIFI_SDPCM_IOCTL_ID_SHIFT),
                  (unsigned long)payloadLength,
                  (unsigned long)status,
                  (unsigned long)g_wifiLastIoctlValue);

    if (((flags & APP_WIFI_SDPCM_IOCTL_ID_MASK) >> APP_WIFI_SDPCM_IOCTL_ID_SHIFT) == g_wifiIoctlRequestId)
    {
      g_wifiIoctlProbeCompleted = 1U;

      if ((command == APP_WIFI_WLC_GET_VERSION) && (status == 0U))
      {
        g_wifiVersionProbeCompleted = 1U;
      }

      if ((command == APP_WIFI_WLC_UP) && (status == 0U))
      {
        g_wifiUpProbeCompleted = 1U;
      }

      if ((command == APP_WIFI_WLC_GET_VAR) && (status == 0U))
      {
        g_wifiMacProbeCompleted = 1U;
      }

      if ((command == APP_WIFI_WLC_SET_VAR) && (status == 0U))
      {
        g_wifiEventMaskCompleted = 1U;
      }

      if ((command == APP_WIFI_WLC_SET_PM) && (status == 0U))
      {
        g_wifiPmProbeCompleted = 1U;
      }
    }
  }
  if ((frameLength == APP_WIFI_SDPCM_HEADER_SIZE) &&
      (g_wifiLastSdpcmHeaderLength == APP_WIFI_SDPCM_HEADER_SIZE))
  {
    APP_WiFi_Logf("[wifi] sdpcm: credit-only seq=%u ch=%s credit=%u diff=%u flow=%u\n",
                  (unsigned int)g_wifiLastSdpcmSequence,
                  APP_WiFi_SdpcmChannelToString(g_wifiLastSdpcmChannel),
                  (unsigned int)g_wifiBusCredit,
                  (unsigned int)g_wifiBusCreditDiff,
                  (unsigned int)g_wifiLastSdpcmFlowControl);
  }
  (void)APP_WiFi_Platform_ClearFunction2RxInterrupt(frameInterrupt);
  return 1U;
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
    uint32_t drainCount = 0U;

    for (drainCount = 0U; drainCount < 4U; ++drainCount)
    {
      if (APP_WiFi_TryProbeSdpcmRx() == 0U)
      {
        break;
      }
    }

    if (g_wifiIoctlProbeSent == 0U)
    {
      (void)APP_WiFi_SendGetVersionIoctl();
    }
    else if ((g_wifiVersionProbeCompleted != 0U) &&
             (g_wifiUpProbeSent == 0U) &&
             (g_wifiIoctlProbeCompleted != 0U))
    {
      if (APP_WiFi_SendUpIoctl() == HAL_OK)
      {
        g_wifiUpProbeSent = 1U;
      }
    }
    else if ((g_wifiUpProbeCompleted != 0U) &&
             (g_wifiMacProbeSent == 0U) &&
             (g_wifiIoctlProbeCompleted != 0U))
    {
      if (APP_WiFi_SendGetCurEtheraddrIovar() == HAL_OK)
      {
        g_wifiMacProbeSent = 1U;
      }
    }
    else if ((g_wifiMacProbeCompleted != 0U) &&
             (g_wifiEventMaskSent == 0U) &&
             (g_wifiIoctlProbeCompleted != 0U))
    {
      if (APP_WiFi_SendSetEventMsgsIovar() == HAL_OK)
      {
        g_wifiEventMaskSent = 1U;
      }
    }
    else if ((g_wifiEventMaskCompleted != 0U) &&
             (g_wifiPmProbeSent == 0U) &&
             (g_wifiIoctlProbeCompleted != 0U))
    {
      if (APP_WiFi_SendSetPmOffIoctl() == HAL_OK)
      {
        g_wifiPmProbeSent = 1U;
      }
    }

    (void)APP_WiFi_Platform_ProbeConsole();
    (void)APP_WiFi_Platform_ProbeMailbox();

    APP_WiFi_Logf("[wifi] heartbeat: state=%s sdioIrq=%lu oob=%lu mailbox=0x%08lX int=0x%08lX consoleWr=%lu consoleOut=%lu sdpcm=%lu len=%u ch=%u seq=%u credit=%u flow=%u ioctlSent=%u ioctlDone=%u ioctlCmd=%lu ioctlStatus=0x%08lX verOk=%u upSent=%u upOk=%u macSent=%u macOk=%u evtSent=%u evtOk=%u pmSent=%u pmOk=%u\n",
                  APP_WiFi_StateToString(g_wifiState),
                  (unsigned long)APP_WiFi_Platform_GetSdioInterruptCount(),
                  (unsigned long)APP_WiFi_GetOobInterruptCount(),
                  (unsigned long)APP_WiFi_Platform_GetHostMailboxData(),
                  (unsigned long)APP_WiFi_Platform_GetInterruptStatus(),
                  (unsigned long)APP_WiFi_Platform_GetConsoleWriteIndex(),
                  (unsigned long)APP_WiFi_Platform_GetConsoleOutIndex(),
                  (unsigned long)g_wifiSdpcmFrameCount,
                  (unsigned int)g_wifiLastSdpcmFrameLength,
                  (unsigned int)g_wifiLastSdpcmChannel,
                  (unsigned int)g_wifiLastSdpcmSequence,
                  (unsigned int)g_wifiBusCredit,
                  (unsigned int)g_wifiLastSdpcmFlowControl,
                  (unsigned int)g_wifiIoctlProbeSent,
                  (unsigned int)g_wifiIoctlProbeCompleted,
                  (unsigned long)g_wifiLastIoctlCommand,
                  (unsigned long)g_wifiLastIoctlStatus,
                  (unsigned int)g_wifiVersionProbeCompleted,
                  (unsigned int)g_wifiUpProbeSent,
                  (unsigned int)g_wifiUpProbeCompleted,
                  (unsigned int)g_wifiMacProbeSent,
                  (unsigned int)g_wifiMacProbeCompleted,
                  (unsigned int)g_wifiEventMaskSent,
                  (unsigned int)g_wifiEventMaskCompleted,
                  (unsigned int)g_wifiPmProbeSent,
                  (unsigned int)g_wifiPmProbeCompleted);
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
