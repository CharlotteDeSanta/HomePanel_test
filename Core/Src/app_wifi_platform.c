#include "app_wifi_platform.h"

#include "app_wifi_resources.h"
#include "cmsis_os2.h"
#include <string.h>
#include "main.h"
#include "sdmmc.h"
#include "stm32h7xx_ll_sdmmc.h"

#define APP_WIFI_SDIO_ENUM_TIMEOUT_MS 500U
#define APP_WIFI_SDIO_FN1_ENABLE_TIMEOUT_MS 500U
#define APP_WIFI_SDIO_CMD53_TIMEOUT_MS 100U
#define APP_WIFI_SDIO_FN0             0U
#define APP_WIFI_SDIO_FN1             1U
#define APP_WIFI_SDIO_CCCR_REV        0x00U
#define APP_WIFI_SDIO_CCCR_SDREV      0x01U
#define APP_WIFI_SDIO_CCCR_IOEN       0x02U
#define APP_WIFI_SDIO_CCCR_IORDY      0x03U
#define APP_WIFI_SDIO_CCCR_INTEN      0x04U
#define APP_WIFI_SDIO_CCCR_BICTRL     0x07U
#define APP_WIFI_SDIO_CCCR_CAPS       0x08U
#define APP_WIFI_SDIO_SEP_INT_CTL     0xF2U
#define APP_WIFI_SDIO_CCCR_BLKSIZE_0  0x10U
#define APP_WIFI_SDIO_CCCR_BLKSIZE_1  0x11U
#define APP_WIFI_SDIO_CCCR_F1BLKSIZE_0 0x110U
#define APP_WIFI_SDIO_CCCR_F1BLKSIZE_1 0x111U
#define APP_WIFI_SDIO_BLOCK_SIZE      64U
#define APP_WIFI_SDIO_FUNC_ENABLE_1   0x02U
#define APP_WIFI_SDIO_FUNC_ENABLE_2   0x04U
#define APP_WIFI_SDIO_FUNC_READY_1    0x02U
#define APP_WIFI_SDIO_FUNC_READY_2    0x04U
#define APP_WIFI_SDIO_BUS_WIDTH_MASK  0x03U
#define APP_WIFI_SDIO_BUS_WIDTH_4BIT  0x02U
#define APP_WIFI_SDIO_INTERRUPT_MASTER_EN 0x01U
#define APP_WIFI_SDIO_INTERRUPT_FUNC1_EN  0x02U
#define APP_WIFI_SDIO_INTERRUPT_FUNC2_EN  0x04U
#define APP_WIFI_SDIO_SEP_INTR_MASK       0x01U
#define APP_WIFI_SDIO_SEP_INTR_EN         0x02U
#define APP_WIFI_SDIO_SEP_INTR_POL        0x04U
#define APP_WIFI_SDIO_FUNCTION2_WATERMARK 0x10008U
#define APP_WIFI_SDIO_BACKPLANE_ADDRESS_LOW  0x1000AU
#define APP_WIFI_SDIO_BACKPLANE_ADDRESS_MID  0x1000BU
#define APP_WIFI_SDIO_BACKPLANE_ADDRESS_HIGH 0x1000CU
#define APP_WIFI_SDIO_CHIP_CLOCK_CSR  0x1000EU
#define APP_WIFI_SDIO_PULL_UP         0x1000FU
#define APP_WIFI_SDIO_CMD53_SMOKE_LEN 4U
#define APP_WIFI_SDIO_ALP_TIMEOUT_MS  500U
#define APP_WIFI_SDIO_HT_TIMEOUT_MS   500U
#define APP_WIFI_SDIO_FN2_ENABLE_TIMEOUT_MS 1000U
#define APP_WIFI_SDIO_FORCE_ALP       0x01U
#define APP_WIFI_SDIO_FORCE_HT        0x02U
#define APP_WIFI_SDIO_ALP_AVAIL_REQ   0x08U
#define APP_WIFI_SDIO_HT_AVAIL_REQ    0x10U
#define APP_WIFI_SDIO_FORCE_HW_CLKREQ_OFF 0x20U
#define APP_WIFI_SDIO_ALP_AVAIL       0x40U
#define APP_WIFI_SDIO_HT_AVAIL        0x80U
#define APP_WIFI_BACKPLANE_ADDRESS_MASK 0x7FFFU
#define APP_WIFI_BACKPLANE_WINDOW_SIZE (APP_WIFI_BACKPLANE_ADDRESS_MASK + 1U)
#define APP_WIFI_BACKPLANE_WINDOW_INVALID 0xFFFFFFFFU
#define APP_WIFI_CHIPCOMMON_BASE_ADDRESS 0x18000000U
#define APP_WIFI_CHIPCOMMON_GPIO_CONTROL (APP_WIFI_CHIPCOMMON_BASE_ADDRESS + 0x6CU)
#define APP_WIFI_SDIO_INT_HOST_MASK_ADDRESS   0x18002024U
#define APP_WIFI_SDIO_FUNCTION_INT_MASK_ADDRESS 0x18002034U
#define APP_WIFI_HOST_INT_MASK_VALUE 0x000000F0U
#define APP_WIFI_FUNCTION_INT_MASK_VALUE 0x03U
#define APP_WIFI_43362_RAM_BASE 0x00000000U
#define APP_WIFI_43362_RAM_SIZE 0x0003C000U
#define APP_WIFI_WLAN_SHARED_PTR_ADDRESS (APP_WIFI_43362_RAM_BASE + APP_WIFI_43362_RAM_SIZE - sizeof(uint32_t))
#define APP_WIFI_WLAN_SHARED_VERSION_MASK 0x000000FFU
#define APP_WIFI_WLAN_SHARED_VERSION      0x00000001U
#define APP_WIFI_NVRAM_TRAILER_SIZE 4U
#define APP_WIFI_STAGE_CHUNK_SIZE 1024U
#define APP_WIFI_WLAN_ARM_WRAPPER_BASE 0x18103000U
#define APP_WIFI_SDIO_INT_STATUS_ADDRESS 0x18002020U
#define APP_WIFI_SDIO_TO_HOST_MAILBOX_DATA_ADDRESS 0x1800204CU
#define APP_WIFI_AI_IOCTRL_OFFSET 0x408U
#define APP_WIFI_AI_RESETCTRL_OFFSET 0x800U
#define APP_WIFI_SICF_CLOCK_EN 0x01U
#define APP_WIFI_SICF_FGC 0x02U
#define APP_WIFI_SICF_CPUHALT 0x20U
#define APP_WIFI_AIRC_RESET 0x01U
#define APP_WIFI_I_HMB_HOST_INT 0x00000080U
#define APP_WIFI_FIRMWARE_BOOT_TIMEOUT_MS 500U
#define APP_WIFI_CONSOLE_LOG_OFFSET (sizeof(uint32_t) * 2U)

typedef struct
{
  uint32_t flags;
  uint32_t trapAddress;
  uint32_t assertExpressionAddress;
  uint32_t assertFileAddress;
  uint32_t assertLine;
  uint32_t consoleAddress;
  uint32_t msgtraceAddress;
  uint32_t firmwareId;
} APP_WiFi_WlanShared_t;

typedef struct
{
  uint32_t bufferAddress;
  uint32_t bufferSize;
  uint32_t writeIndex;
  uint32_t outIndex;
} APP_WiFi_HndLog_t;

typedef HAL_StatusTypeDef (*APP_WiFi_ResourceReadFunc_t)(uint32_t offset, uint8_t *buffer, uint32_t bufferSize, uint32_t *sizeRead);

static volatile uint32_t g_wifiSdioInterruptCount = 0U;
static volatile uint32_t g_wifiLastSdioStatus = 0U;
static volatile uint32_t g_wifiLastSdioError = SDMMC_ERROR_NONE;
static volatile uint32_t g_wifiSdioOcr = 0U;
static volatile uint16_t g_wifiSdioRca = 0U;
static volatile uint32_t g_wifiSdioHostInitialized = 0U;
static volatile uint32_t g_wifiSdioEnumerated = 0U;
static volatile uint32_t g_wifiSdioBusConfigured = 0U;
static volatile uint32_t g_wifiLastCmd53Word = 0U;
static volatile uint32_t g_wifiLastBackplaneWord = 0U;
static volatile uint32_t g_wifiBackplaneWindowBase = APP_WIFI_BACKPLANE_WINDOW_INVALID;
static volatile uint32_t g_wifiHostInterruptMask = 0U;
static volatile uint32_t g_wifiFunctionInterruptMask = 0U;
static volatile uint32_t g_wifiFirmwareSize = 0U;
static volatile uint32_t g_wifiFirmwareEntryWord = 0U;
static volatile uint32_t g_wifiNvramSize = 0U;
static volatile uint32_t g_wifiNvramStagingAddress = 0U;
static volatile uint32_t g_wifiNvramTrailerWord = 0U;
static volatile uint32_t g_wifiFirmwareBytesStaged = 0U;
static volatile uint32_t g_wifiNvramBytesStaged = 0U;
static volatile uint32_t g_wifiWlanSharedAddress = 0U;
static volatile uint32_t g_wifiWlanSharedFlags = 0U;
static volatile uint32_t g_wifiTrapAddress = 0U;
static volatile uint32_t g_wifiAssertExpressionAddress = 0U;
static volatile uint32_t g_wifiAssertFileAddress = 0U;
static volatile uint32_t g_wifiAssertLine = 0U;
static volatile uint32_t g_wifiConsoleAddress = 0U;
static volatile uint32_t g_wifiMsgtraceAddress = 0U;
static volatile uint32_t g_wifiFirmwareId = 0U;
static volatile uint32_t g_wifiConsoleBufferAddress = 0U;
static volatile uint32_t g_wifiConsoleBufferSize = 0U;
static volatile uint32_t g_wifiConsoleWriteIndex = 0U;
static volatile uint32_t g_wifiConsoleOutIndex = 0U;
static volatile uint32_t g_wifiInterruptStatus = 0U;
static volatile uint32_t g_wifiHostMailboxData = 0U;
static volatile uint8_t g_wifiChipClockCsr = 0U;
static volatile uint8_t g_wifiWlanCoreIoCtrl = 0U;
static volatile uint8_t g_wifiWlanCoreResetCtrl = 0U;
static volatile uint8_t g_wifiCccrInterruptEnable = 0U;
static volatile uint8_t g_wifiSepInterruptControl = 0U;
static volatile uint8_t g_wifiCccrIoEnable = 0U;
static volatile uint8_t g_wifiCccrBusControl = 0U;
static volatile uint8_t g_wifiCccrRevision = 0U;
static volatile uint8_t g_wifiCccrSdRevision = 0U;
static volatile uint8_t g_wifiCccrIoReady = 0U;
static volatile uint8_t g_wifiCccrCapabilities = 0U;
static uint32_t g_wifiCmd53Scratch[(APP_WIFI_STAGE_CHUNK_SIZE + sizeof(uint32_t) - 1U) / sizeof(uint32_t)] = {0};
static uint8_t g_wifiStageBuffer[APP_WIFI_STAGE_CHUNK_SIZE] = {0};

static HAL_StatusTypeDef APP_WiFi_Platform_SdioErrorToHalStatus(uint32_t error);
static HAL_StatusTypeDef APP_WiFi_Platform_ApplyHostBusWidth(uint32_t busWide);
static HAL_StatusTypeDef APP_WiFi_Platform_WaitCmd53Transfer(void);
static HAL_StatusTypeDef APP_WiFi_Platform_Cmd53Write(uint8_t functionNumber, uint32_t address, const uint8_t *data, uint16_t dataLength);
static HAL_StatusTypeDef APP_WiFi_Platform_BackplaneRead8(uint32_t address, uint8_t *value);
static HAL_StatusTypeDef APP_WiFi_Platform_BackplaneWrite8(uint32_t address, uint8_t value);
static HAL_StatusTypeDef APP_WiFi_Platform_ReadCoreState(uint32_t wrapperBase, uint8_t *ioCtrl, uint8_t *resetCtrl);
static HAL_StatusTypeDef APP_WiFi_Platform_DisableDeviceCore(uint32_t wrapperBase, uint8_t cpuHalt);
static HAL_StatusTypeDef APP_WiFi_Platform_ResetDeviceCore(uint32_t wrapperBase, uint8_t cpuHalt);
static HAL_StatusTypeDef APP_WiFi_Platform_DeviceCoreIsUp(uint32_t wrapperBase);
static HAL_StatusTypeDef APP_WiFi_Platform_WriteResourceToBackplane(APP_WiFi_ResourceReadFunc_t readFunc,
                                                                    uint32_t resourceSize,
                                                                    uint32_t destinationAddress,
                                                                    uint32_t paddedSize,
                                                                    uint32_t *bytesStaged);
static uint32_t APP_WiFi_Platform_BuildCmd53Argument(uint8_t rwFlag,
                                                     uint8_t functionNumber,
                                                     uint32_t address,
                                                     uint8_t blockMode,
                                                     uint8_t opCode,
                                                     uint16_t count);
static uint32_t APP_WiFi_Platform_BuildCmd52Argument(uint8_t rwFlag,
                                                     uint8_t functionNumber,
                                                     uint32_t address,
                                                     uint8_t rawFlag,
                                                     uint8_t value);
static uint32_t APP_WiFi_Platform_RoundTransferLength(uint16_t dataLength);
static uint32_t APP_WiFi_Platform_GetDctrlBlockSize(uint32_t transferLength);

static void APP_WiFi_Platform_SdioGpioInit(void)
{
  GPIO_InitTypeDef gpioInit = {0};

  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

  gpioInit.Pin = GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12;
  gpioInit.Mode = GPIO_MODE_AF_PP;
  gpioInit.Pull = GPIO_PULLUP;
  gpioInit.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  gpioInit.Alternate = GPIO_AF12_SDIO1;
  HAL_GPIO_Init(GPIOC, &gpioInit);

  gpioInit.Pin = GPIO_PIN_2;
  HAL_GPIO_Init(GPIOD, &gpioInit);
}

void APP_WiFi_Platform_Init(void)
{
  g_wifiSdioInterruptCount = 0U;
  g_wifiLastSdioStatus = 0U;
  g_wifiLastSdioError = SDMMC_ERROR_NONE;
  g_wifiSdioOcr = 0U;
  g_wifiSdioRca = 0U;
  g_wifiSdioHostInitialized = 0U;
  g_wifiSdioEnumerated = 0U;
  g_wifiSdioBusConfigured = 0U;
  g_wifiLastCmd53Word = 0U;
  g_wifiLastBackplaneWord = 0U;
  g_wifiBackplaneWindowBase = APP_WIFI_BACKPLANE_WINDOW_INVALID;
  g_wifiHostInterruptMask = 0U;
  g_wifiFunctionInterruptMask = 0U;
  g_wifiFirmwareSize = 0U;
  g_wifiFirmwareEntryWord = 0U;
  g_wifiNvramSize = 0U;
  g_wifiNvramStagingAddress = 0U;
  g_wifiNvramTrailerWord = 0U;
  g_wifiFirmwareBytesStaged = 0U;
  g_wifiNvramBytesStaged = 0U;
  g_wifiWlanSharedAddress = 0U;
  g_wifiWlanSharedFlags = 0U;
  g_wifiTrapAddress = 0U;
  g_wifiAssertExpressionAddress = 0U;
  g_wifiAssertFileAddress = 0U;
  g_wifiAssertLine = 0U;
  g_wifiConsoleAddress = 0U;
  g_wifiMsgtraceAddress = 0U;
  g_wifiFirmwareId = 0U;
  g_wifiConsoleBufferAddress = 0U;
  g_wifiConsoleBufferSize = 0U;
  g_wifiConsoleWriteIndex = 0U;
  g_wifiConsoleOutIndex = 0U;
  g_wifiInterruptStatus = 0U;
  g_wifiHostMailboxData = 0U;
  g_wifiChipClockCsr = 0U;
  g_wifiWlanCoreIoCtrl = 0U;
  g_wifiWlanCoreResetCtrl = 0U;
  g_wifiCccrInterruptEnable = 0U;
  g_wifiSepInterruptControl = 0U;
  g_wifiCccrIoEnable = 0U;
  g_wifiCccrBusControl = 0U;
  g_wifiCccrRevision = 0U;
  g_wifiCccrSdRevision = 0U;
  g_wifiCccrIoReady = 0U;
  g_wifiCccrCapabilities = 0U;
  HAL_GPIO_WritePin(WIFI_RESET_GPIO_Port, WIFI_RESET_Pin, GPIO_PIN_SET);
}

void APP_WiFi_Platform_SetResetPin(GPIO_PinState pinState)
{
  HAL_GPIO_WritePin(WIFI_RESET_GPIO_Port, WIFI_RESET_Pin, pinState);
}

HAL_StatusTypeDef APP_WiFi_Platform_SdioHostInit(void)
{
  if (g_wifiSdioHostInitialized != 0U)
  {
    return HAL_OK;
  }

  APP_WiFi_Platform_SdioGpioInit();

  (void)SDMMC_PowerState_OFF(SDMMC1);
  __HAL_RCC_SDMMC1_FORCE_RESET();
  __HAL_RCC_SDMMC1_RELEASE_RESET();
  __HAL_RCC_SDMMC1_CLK_ENABLE();

  HAL_NVIC_SetPriority(SDMMC1_IRQn, 6, 0);
  HAL_NVIC_EnableIRQ(SDMMC1_IRQn);

  SDMMC1->MASK = 0U;
  SDMMC1->ICR = 0xFFFFFFFFU;

  if (APP_WiFi_Platform_ApplyHostBusWidth(SDMMC_BUS_WIDE_1B) != HAL_OK)
  {
    return HAL_ERROR;
  }

  g_wifiSdioHostInitialized = 1U;

  return HAL_OK;
}

HAL_StatusTypeDef APP_WiFi_Platform_SdioEnumerate(void)
{
  uint32_t attempt = 0U;

  if (g_wifiSdioHostInitialized == 0U)
  {
    return HAL_ERROR;
  }

  if (g_wifiSdioEnumerated != 0U)
  {
    return HAL_OK;
  }

  for (attempt = 0U; attempt < APP_WIFI_SDIO_ENUM_TIMEOUT_MS; ++attempt)
  {
    uint32_t error = SDMMC_CmdGoIdleState(SDMMC1);
    uint32_t ocr = 0U;
    uint16_t rca = 0U;

    if (error != SDMMC_ERROR_NONE)
    {
      g_wifiLastSdioError = error;
      osDelay(1U);
      continue;
    }

    error = SDMMC_CmdSendOperationcondition(SDMMC1, 0U, &ocr);
    if (error != SDMMC_ERROR_NONE)
    {
      g_wifiLastSdioError = error;
      osDelay(1U);
      continue;
    }

    error = SDMMC_CmdSetRelAdd(SDMMC1, &rca);
    if ((error != SDMMC_ERROR_NONE) || (rca == 0U))
    {
      g_wifiLastSdioError = (error != SDMMC_ERROR_NONE) ? error : SDMMC_ERROR_TIMEOUT;
      osDelay(1U);
      continue;
    }

    error = SDMMC_CmdSelDesel(SDMMC1, ((uint32_t)rca) << 16U);
    if (error != SDMMC_ERROR_NONE)
    {
      g_wifiLastSdioError = error;
      osDelay(1U);
      continue;
    }

    g_wifiSdioOcr = ocr;
    g_wifiSdioRca = rca;
    g_wifiLastSdioStatus = SDMMC1->STA;
    g_wifiLastSdioError = SDMMC_ERROR_NONE;
    g_wifiSdioEnumerated = 1U;
    return HAL_OK;
  }

  return HAL_TIMEOUT;
}

HAL_StatusTypeDef APP_WiFi_Platform_Cmd52Read(uint8_t functionNumber, uint32_t address, uint8_t *value)
{
  uint8_t response = 0U;
  const uint32_t argument = APP_WiFi_Platform_BuildCmd52Argument(0U, functionNumber, address, 0U, 0U);
  const uint32_t error = SDMMC_SDIO_CmdReadWriteDirect(SDMMC1, argument, &response);

  g_wifiLastSdioStatus = SDMMC1->STA;
  g_wifiLastSdioError = error;

  if (error != SDMMC_ERROR_NONE)
  {
    return APP_WiFi_Platform_SdioErrorToHalStatus(error);
  }

  if (value != NULL)
  {
    *value = response;
  }

  return HAL_OK;
}

HAL_StatusTypeDef APP_WiFi_Platform_Cmd52Write(uint8_t functionNumber, uint32_t address, uint8_t value)
{
  uint8_t response = 0U;
  const uint32_t argument = APP_WiFi_Platform_BuildCmd52Argument(1U, functionNumber, address, 0U, value);
  const uint32_t error = SDMMC_SDIO_CmdReadWriteDirect(SDMMC1, argument, &response);

  g_wifiLastSdioStatus = SDMMC1->STA;
  g_wifiLastSdioError = error;

  if (error != SDMMC_ERROR_NONE)
  {
    return APP_WiFi_Platform_SdioErrorToHalStatus(error);
  }

  return HAL_OK;
}

HAL_StatusTypeDef APP_WiFi_Platform_ProbeCccr(void)
{
  uint8_t value = 0U;

  if (g_wifiSdioEnumerated == 0U)
  {
    return HAL_ERROR;
  }

  if (APP_WiFi_Platform_Cmd52Read(APP_WIFI_SDIO_FN0, APP_WIFI_SDIO_CCCR_REV, &value) != HAL_OK)
  {
    return HAL_ERROR;
  }
  g_wifiCccrRevision = value;

  if (APP_WiFi_Platform_Cmd52Read(APP_WIFI_SDIO_FN0, APP_WIFI_SDIO_CCCR_SDREV, &value) != HAL_OK)
  {
    return HAL_ERROR;
  }
  g_wifiCccrSdRevision = value;

  if (APP_WiFi_Platform_Cmd52Read(APP_WIFI_SDIO_FN0, APP_WIFI_SDIO_CCCR_IOEN, &value) != HAL_OK)
  {
    return HAL_ERROR;
  }
  g_wifiCccrIoEnable = value;

  if (APP_WiFi_Platform_Cmd52Read(APP_WIFI_SDIO_FN0, APP_WIFI_SDIO_CCCR_IORDY, &value) != HAL_OK)
  {
    return HAL_ERROR;
  }
  g_wifiCccrIoReady = value;

  if (APP_WiFi_Platform_Cmd52Read(APP_WIFI_SDIO_FN0, APP_WIFI_SDIO_CCCR_BICTRL, &value) != HAL_OK)
  {
    return HAL_ERROR;
  }
  g_wifiCccrBusControl = value;

  if (APP_WiFi_Platform_Cmd52Read(APP_WIFI_SDIO_FN0, APP_WIFI_SDIO_CCCR_CAPS, &value) != HAL_OK)
  {
    return HAL_ERROR;
  }
  g_wifiCccrCapabilities = value;

  return HAL_OK;
}

HAL_StatusTypeDef APP_WiFi_Platform_EnableFunction1(void)
{
  uint32_t attempt = 0U;
  uint8_t ioEnable = 0U;
  uint8_t ioReady = 0U;

  if (g_wifiSdioEnumerated == 0U)
  {
    return HAL_ERROR;
  }

  for (attempt = 0U; attempt < APP_WIFI_SDIO_FN1_ENABLE_TIMEOUT_MS; ++attempt)
  {
    if (APP_WiFi_Platform_Cmd52Read(APP_WIFI_SDIO_FN0, APP_WIFI_SDIO_CCCR_IOEN, &ioEnable) != HAL_OK)
    {
      osDelay(1U);
      continue;
    }

    ioEnable = (uint8_t)(ioEnable | APP_WIFI_SDIO_FUNC_ENABLE_1);
    if (APP_WiFi_Platform_Cmd52Write(APP_WIFI_SDIO_FN0, APP_WIFI_SDIO_CCCR_IOEN, ioEnable) != HAL_OK)
    {
      osDelay(1U);
      continue;
    }

    if (APP_WiFi_Platform_Cmd52Read(APP_WIFI_SDIO_FN0, APP_WIFI_SDIO_CCCR_IOEN, &ioEnable) != HAL_OK)
    {
      osDelay(1U);
      continue;
    }
    g_wifiCccrIoEnable = ioEnable;

    if ((ioEnable & APP_WIFI_SDIO_FUNC_ENABLE_1) == 0U)
    {
      osDelay(1U);
      continue;
    }

    if (APP_WiFi_Platform_Cmd52Read(APP_WIFI_SDIO_FN0, APP_WIFI_SDIO_CCCR_IORDY, &ioReady) != HAL_OK)
    {
      osDelay(1U);
      continue;
    }
    g_wifiCccrIoReady = ioReady;

    if ((ioReady & APP_WIFI_SDIO_FUNC_READY_1) != 0U)
    {
      return HAL_OK;
    }

    osDelay(1U);
  }

  g_wifiLastSdioError = SDMMC_ERROR_TIMEOUT;
  return HAL_TIMEOUT;
}

HAL_StatusTypeDef APP_WiFi_Platform_ConfigureBus(void)
{
  uint8_t value = 0U;

  if (g_wifiSdioEnumerated == 0U)
  {
    return HAL_ERROR;
  }

  if (g_wifiSdioBusConfigured != 0U)
  {
    return HAL_OK;
  }

  if (APP_WiFi_Platform_Cmd52Write(APP_WIFI_SDIO_FN0, APP_WIFI_SDIO_CCCR_BLKSIZE_0, APP_WIFI_SDIO_BLOCK_SIZE) != HAL_OK)
  {
    return HAL_ERROR;
  }

  if (APP_WiFi_Platform_Cmd52Write(APP_WIFI_SDIO_FN0, APP_WIFI_SDIO_CCCR_BLKSIZE_1, 0U) != HAL_OK)
  {
    return HAL_ERROR;
  }

  if (APP_WiFi_Platform_Cmd52Write(APP_WIFI_SDIO_FN0, APP_WIFI_SDIO_CCCR_F1BLKSIZE_0, APP_WIFI_SDIO_BLOCK_SIZE) != HAL_OK)
  {
    return HAL_ERROR;
  }

  if (APP_WiFi_Platform_Cmd52Write(APP_WIFI_SDIO_FN0, APP_WIFI_SDIO_CCCR_F1BLKSIZE_1, 0U) != HAL_OK)
  {
    return HAL_ERROR;
  }

  if (APP_WiFi_Platform_Cmd52Read(APP_WIFI_SDIO_FN0, APP_WIFI_SDIO_CCCR_BICTRL, &value) != HAL_OK)
  {
    return HAL_ERROR;
  }

  value = (uint8_t)((value & (uint8_t)(~APP_WIFI_SDIO_BUS_WIDTH_MASK)) | APP_WIFI_SDIO_BUS_WIDTH_4BIT);
  if (APP_WiFi_Platform_Cmd52Write(APP_WIFI_SDIO_FN0, APP_WIFI_SDIO_CCCR_BICTRL, value) != HAL_OK)
  {
    return HAL_ERROR;
  }

  if (APP_WiFi_Platform_Cmd52Read(APP_WIFI_SDIO_FN0, APP_WIFI_SDIO_CCCR_BICTRL, &value) != HAL_OK)
  {
    return HAL_ERROR;
  }
  g_wifiCccrBusControl = value;

  if (APP_WiFi_Platform_ApplyHostBusWidth(SDMMC_BUS_WIDE_4B) != HAL_OK)
  {
    return HAL_ERROR;
  }

  g_wifiSdioBusConfigured = 1U;
  return HAL_OK;
}

HAL_StatusTypeDef APP_WiFi_Platform_Cmd53Read(uint8_t functionNumber, uint32_t address, uint8_t *data, uint16_t dataLength)
{
  const uint32_t transferLength = APP_WiFi_Platform_RoundTransferLength(dataLength);
  const uint32_t dctrlBlockSize = APP_WiFi_Platform_GetDctrlBlockSize(transferLength);
  const uint32_t argument = APP_WiFi_Platform_BuildCmd53Argument(0U,
                                                                 functionNumber,
                                                                 address,
                                                                 0U,
                                                                 1U,
                                                                 (uint16_t)transferLength);

  if ((data == NULL) || (dataLength == 0U) || (transferLength == 0U) || (dctrlBlockSize == 0U) ||
      (transferLength > sizeof(g_wifiCmd53Scratch)))
  {
    return HAL_ERROR;
  }

  memset((void *)g_wifiCmd53Scratch, 0, sizeof(g_wifiCmd53Scratch));

  SDMMC1->MASK = 0U;
  SDMMC1->ICR = 0xFFFFFFFFU;
  SDMMC1->DTIMER = SDMMC_DATATIMEOUT;
  SDMMC1->DLEN = transferLength;
  SDMMC1->DCTRL = dctrlBlockSize | SDMMC_TRANSFER_DIR_TO_SDMMC | SDMMC_TRANSFER_MODE_BLOCK | SDMMC_DCTRL_SDIOEN;
  SDMMC1->IDMACTRL = SDMMC_ENABLE_IDMA_SINGLE_BUFF;
  SDMMC1->IDMABASE0 = (uint32_t)g_wifiCmd53Scratch;

  SDMMC1->ARG = argument;
  SDMMC1->CMD = (uint32_t)(SDMMC_CMD_SDMMC_RW_EXTENDED | SDMMC_RESPONSE_SHORT | SDMMC_WAIT_NO | SDMMC_CPSM_ENABLE | SDMMC_CMD_CMDTRANS);

  if (APP_WiFi_Platform_WaitCmd53Transfer() != HAL_OK)
  {
    return HAL_ERROR;
  }

  memcpy(data, (const void *)g_wifiCmd53Scratch, dataLength);
  return HAL_OK;
}

HAL_StatusTypeDef APP_WiFi_Platform_RunCmd53SmokeTest(void)
{
  uint8_t data[APP_WIFI_SDIO_CMD53_SMOKE_LEN] = {0};

  if (APP_WiFi_Platform_Cmd53Read(APP_WIFI_SDIO_FN1, 0U, data, (uint16_t)sizeof(data)) != HAL_OK)
  {
    return HAL_ERROR;
  }

  g_wifiLastCmd53Word = ((uint32_t)data[0]) |
                        ((uint32_t)data[1] << 8U) |
                        ((uint32_t)data[2] << 16U) |
                        ((uint32_t)data[3] << 24U);
  return HAL_OK;
}

HAL_StatusTypeDef APP_WiFi_Platform_Fn1Read8(uint32_t address, uint8_t *value)
{
  return APP_WiFi_Platform_Cmd52Read(APP_WIFI_SDIO_FN1, address, value);
}

HAL_StatusTypeDef APP_WiFi_Platform_Fn1Write8(uint32_t address, uint8_t value)
{
  return APP_WiFi_Platform_Cmd52Write(APP_WIFI_SDIO_FN1, address, value);
}

HAL_StatusTypeDef APP_WiFi_Platform_RequestAlpClock(void)
{
  const uint8_t requestValue = APP_WIFI_SDIO_FORCE_HW_CLKREQ_OFF |
                               APP_WIFI_SDIO_ALP_AVAIL_REQ |
                               APP_WIFI_SDIO_FORCE_ALP;
  uint32_t attempt = 0U;
  uint8_t chipClockCsr = 0U;

  if (g_wifiSdioBusConfigured == 0U)
  {
    return HAL_ERROR;
  }

  if (APP_WiFi_Platform_Fn1Write8(APP_WIFI_SDIO_CHIP_CLOCK_CSR, requestValue) != HAL_OK)
  {
    return HAL_ERROR;
  }

  for (attempt = 0U; attempt < APP_WIFI_SDIO_ALP_TIMEOUT_MS; ++attempt)
  {
    if (APP_WiFi_Platform_Fn1Read8(APP_WIFI_SDIO_CHIP_CLOCK_CSR, &chipClockCsr) != HAL_OK)
    {
      osDelay(1U);
      continue;
    }

    g_wifiChipClockCsr = chipClockCsr;
    if ((chipClockCsr & APP_WIFI_SDIO_ALP_AVAIL) != 0U)
    {
      (void)APP_WiFi_Platform_Fn1Write8(APP_WIFI_SDIO_CHIP_CLOCK_CSR, 0U);
      return HAL_OK;
    }

    osDelay(1U);
  }

  g_wifiLastSdioError = SDMMC_ERROR_TIMEOUT;
  return HAL_TIMEOUT;
}

HAL_StatusTypeDef APP_WiFi_Platform_SetBackplaneWindow(uint32_t address)
{
  const uint32_t base = address & (uint32_t)(~APP_WIFI_BACKPLANE_ADDRESS_MASK);

  if (base == g_wifiBackplaneWindowBase)
  {
    return HAL_OK;
  }

  if (APP_WiFi_Platform_Fn1Write8(APP_WIFI_SDIO_BACKPLANE_ADDRESS_HIGH, (uint8_t)(base >> 24U)) != HAL_OK)
  {
    return HAL_ERROR;
  }

  if (APP_WiFi_Platform_Fn1Write8(APP_WIFI_SDIO_BACKPLANE_ADDRESS_MID, (uint8_t)(base >> 16U)) != HAL_OK)
  {
    return HAL_ERROR;
  }

  if (APP_WiFi_Platform_Fn1Write8(APP_WIFI_SDIO_BACKPLANE_ADDRESS_LOW, (uint8_t)(base >> 8U)) != HAL_OK)
  {
    return HAL_ERROR;
  }

  g_wifiBackplaneWindowBase = base;
  return HAL_OK;
}

HAL_StatusTypeDef APP_WiFi_Platform_BackplaneRead(uint32_t address, uint8_t *data, uint16_t dataLength)
{
  const uint32_t windowOffset = address & APP_WIFI_BACKPLANE_ADDRESS_MASK;

  if ((data == NULL) || (dataLength == 0U))
  {
    return HAL_ERROR;
  }

  if (((uint32_t)dataLength > APP_WIFI_BACKPLANE_WINDOW_SIZE) ||
      ((windowOffset + (uint32_t)dataLength) > APP_WIFI_BACKPLANE_WINDOW_SIZE))
  {
    return HAL_ERROR;
  }

  if (APP_WiFi_Platform_SetBackplaneWindow(address) != HAL_OK)
  {
    return HAL_ERROR;
  }

  return APP_WiFi_Platform_Cmd53Read(APP_WIFI_SDIO_FN1, windowOffset, data, dataLength);
}

HAL_StatusTypeDef APP_WiFi_Platform_BackplaneRead32(uint32_t address, uint32_t *value)
{
  uint8_t data[sizeof(uint32_t)] = {0};

  if (value == NULL)
  {
    return HAL_ERROR;
  }

  if (APP_WiFi_Platform_BackplaneRead(address, data, (uint16_t)sizeof(data)) != HAL_OK)
  {
    return HAL_ERROR;
  }

  *value = ((uint32_t)data[0]) |
           ((uint32_t)data[1] << 8U) |
           ((uint32_t)data[2] << 16U) |
           ((uint32_t)data[3] << 24U);
  return HAL_OK;
}

HAL_StatusTypeDef APP_WiFi_Platform_BackplaneWrite(uint32_t address, const uint8_t *data, uint16_t dataLength)
{
  const uint32_t windowOffset = address & APP_WIFI_BACKPLANE_ADDRESS_MASK;

  if ((data == NULL) || (dataLength == 0U))
  {
    return HAL_ERROR;
  }

  if (((uint32_t)dataLength > APP_WIFI_BACKPLANE_WINDOW_SIZE) ||
      ((windowOffset + (uint32_t)dataLength) > APP_WIFI_BACKPLANE_WINDOW_SIZE))
  {
    return HAL_ERROR;
  }

  if (APP_WiFi_Platform_SetBackplaneWindow(address) != HAL_OK)
  {
    return HAL_ERROR;
  }

  return APP_WiFi_Platform_Cmd53Write(APP_WIFI_SDIO_FN1, windowOffset, data, dataLength);
}

HAL_StatusTypeDef APP_WiFi_Platform_BackplaneWrite32(uint32_t address, uint32_t value)
{
  uint8_t data[sizeof(uint32_t)] = {0};

  data[0] = (uint8_t)(value & 0xFFU);
  data[1] = (uint8_t)((value >> 8U) & 0xFFU);
  data[2] = (uint8_t)((value >> 16U) & 0xFFU);
  data[3] = (uint8_t)((value >> 24U) & 0xFFU);

  return APP_WiFi_Platform_BackplaneWrite(address, data, (uint16_t)sizeof(data));
}

HAL_StatusTypeDef APP_WiFi_Platform_RunBackplaneSmokeTest(void)
{
  uint32_t value = 0U;

  if (APP_WiFi_Platform_BackplaneRead32(APP_WIFI_CHIPCOMMON_GPIO_CONTROL, &value) != HAL_OK)
  {
    return HAL_ERROR;
  }

  g_wifiLastBackplaneWord = value;
  return HAL_OK;
}

HAL_StatusTypeDef APP_WiFi_Platform_RunBackplaneWriteSmokeTest(void)
{
  uint32_t value = 0U;

  if (APP_WiFi_Platform_BackplaneRead32(APP_WIFI_CHIPCOMMON_GPIO_CONTROL, &value) != HAL_OK)
  {
    return HAL_ERROR;
  }

  if (APP_WiFi_Platform_BackplaneWrite32(APP_WIFI_CHIPCOMMON_GPIO_CONTROL, value) != HAL_OK)
  {
    return HAL_ERROR;
  }

  g_wifiLastBackplaneWord = value;
  return HAL_OK;
}

HAL_StatusTypeDef APP_WiFi_Platform_RequestHtClock(void)
{
  const uint8_t requestValue = APP_WIFI_SDIO_FORCE_HW_CLKREQ_OFF | APP_WIFI_SDIO_HT_AVAIL_REQ;
  uint32_t attempt = 0U;
  uint8_t chipClockCsr = 0U;

  if (g_wifiSdioBusConfigured == 0U)
  {
    return HAL_ERROR;
  }

  if (APP_WiFi_Platform_Fn1Write8(APP_WIFI_SDIO_CHIP_CLOCK_CSR, requestValue) != HAL_OK)
  {
    return HAL_ERROR;
  }

  for (attempt = 0U; attempt < APP_WIFI_SDIO_HT_TIMEOUT_MS; ++attempt)
  {
    if (APP_WiFi_Platform_Fn1Read8(APP_WIFI_SDIO_CHIP_CLOCK_CSR, &chipClockCsr) != HAL_OK)
    {
      osDelay(1U);
      continue;
    }

    g_wifiChipClockCsr = chipClockCsr;
    if ((chipClockCsr & APP_WIFI_SDIO_HT_AVAIL) != 0U)
    {
      return HAL_OK;
    }

    osDelay(1U);
  }

  g_wifiLastSdioError = SDMMC_ERROR_TIMEOUT;
  return HAL_TIMEOUT;
}

HAL_StatusTypeDef APP_WiFi_Platform_EnableFunction2(void)
{
  uint32_t attempt = 0U;
  uint8_t ioEnable = 0U;
  uint8_t ioReady = 0U;

  if (g_wifiSdioEnumerated == 0U)
  {
    return HAL_ERROR;
  }

  for (attempt = 0U; attempt < APP_WIFI_SDIO_FN2_ENABLE_TIMEOUT_MS; ++attempt)
  {
    if (APP_WiFi_Platform_Cmd52Read(APP_WIFI_SDIO_FN0, APP_WIFI_SDIO_CCCR_IOEN, &ioEnable) != HAL_OK)
    {
      osDelay(1U);
      continue;
    }

    ioEnable = (uint8_t)(ioEnable | APP_WIFI_SDIO_FUNC_ENABLE_1 | APP_WIFI_SDIO_FUNC_ENABLE_2);
    if (APP_WiFi_Platform_Cmd52Write(APP_WIFI_SDIO_FN0, APP_WIFI_SDIO_CCCR_IOEN, ioEnable) != HAL_OK)
    {
      osDelay(1U);
      continue;
    }

    if (APP_WiFi_Platform_Cmd52Read(APP_WIFI_SDIO_FN0, APP_WIFI_SDIO_CCCR_IOEN, &ioEnable) != HAL_OK)
    {
      osDelay(1U);
      continue;
    }
    g_wifiCccrIoEnable = ioEnable;

    if ((ioEnable & APP_WIFI_SDIO_FUNC_ENABLE_2) == 0U)
    {
      osDelay(1U);
      continue;
    }

    if (APP_WiFi_Platform_Cmd52Read(APP_WIFI_SDIO_FN0, APP_WIFI_SDIO_CCCR_IORDY, &ioReady) != HAL_OK)
    {
      osDelay(1U);
      continue;
    }
    g_wifiCccrIoReady = ioReady;

    if ((ioReady & APP_WIFI_SDIO_FUNC_READY_2) != 0U)
    {
      return HAL_OK;
    }

    osDelay(1U);
  }

  g_wifiLastSdioError = SDMMC_ERROR_TIMEOUT;
  return HAL_TIMEOUT;
}

HAL_StatusTypeDef APP_WiFi_Platform_ConfigureInterruptPath(void)
{
  uint8_t cccrIntEn = 0U;
  uint8_t sepIntCtl = 0U;
  const uint8_t functionIntMask = APP_WIFI_FUNCTION_INT_MASK_VALUE;

  if (APP_WiFi_Platform_Fn1Write8(APP_WIFI_SDIO_PULL_UP, 0U) != HAL_OK)
  {
    return HAL_ERROR;
  }

  sepIntCtl = (uint8_t)(APP_WIFI_SDIO_SEP_INTR_MASK | APP_WIFI_SDIO_SEP_INTR_EN | APP_WIFI_SDIO_SEP_INTR_POL);
  if (APP_WiFi_Platform_Cmd52Write(APP_WIFI_SDIO_FN0, APP_WIFI_SDIO_SEP_INT_CTL, sepIntCtl) != HAL_OK)
  {
    return HAL_ERROR;
  }
  g_wifiSepInterruptControl = sepIntCtl;

  cccrIntEn = (uint8_t)(APP_WIFI_SDIO_INTERRUPT_MASTER_EN | APP_WIFI_SDIO_INTERRUPT_FUNC2_EN);
  if (APP_WiFi_Platform_Cmd52Write(APP_WIFI_SDIO_FN0, APP_WIFI_SDIO_CCCR_INTEN, cccrIntEn) != HAL_OK)
  {
    return HAL_ERROR;
  }

  if (APP_WiFi_Platform_Cmd52Read(APP_WIFI_SDIO_FN0, APP_WIFI_SDIO_CCCR_INTEN, &cccrIntEn) != HAL_OK)
  {
    return HAL_ERROR;
  }
  g_wifiCccrInterruptEnable = cccrIntEn;

  if (APP_WiFi_Platform_Fn1Write8(APP_WIFI_SDIO_FUNCTION2_WATERMARK, 8U) != HAL_OK)
  {
    return HAL_ERROR;
  }

  if (APP_WiFi_Platform_BackplaneWrite32(APP_WIFI_SDIO_INT_HOST_MASK_ADDRESS, APP_WIFI_HOST_INT_MASK_VALUE) != HAL_OK)
  {
    return HAL_ERROR;
  }
  g_wifiHostInterruptMask = APP_WIFI_HOST_INT_MASK_VALUE;

  if (APP_WiFi_Platform_BackplaneWrite(APP_WIFI_SDIO_FUNCTION_INT_MASK_ADDRESS, &functionIntMask, 1U) != HAL_OK)
  {
    return HAL_ERROR;
  }
  g_wifiFunctionInterruptMask = functionIntMask;

  return HAL_OK;
}

HAL_StatusTypeDef APP_WiFi_Platform_ProbeFirmwareResources(void)
{
  const uint8_t *firmwareData = NULL;
  const uint8_t *nvramData = NULL;
  uint32_t firmwareSize = 0U;
  uint32_t nvramSize = 0U;
  uint32_t roundedNvramSize = 0U;
  uint32_t nvramWordCount = 0U;

  if (APP_WiFi_Resources_GetFirmware(&firmwareData, &firmwareSize) != HAL_OK)
  {
    return HAL_ERROR;
  }

  if ((firmwareData == NULL) || (firmwareSize < sizeof(uint32_t)) || (firmwareSize > APP_WIFI_43362_RAM_SIZE))
  {
    return HAL_ERROR;
  }

  if (APP_WiFi_Resources_GetNvram(&nvramData, &nvramSize) != HAL_OK)
  {
    return HAL_ERROR;
  }

  if ((nvramData == NULL) || (nvramSize == 0U))
  {
    return HAL_ERROR;
  }

  roundedNvramSize = (nvramSize + 3U) & ~0x3U;
  if ((roundedNvramSize + APP_WIFI_NVRAM_TRAILER_SIZE) > APP_WIFI_43362_RAM_SIZE)
  {
    return HAL_ERROR;
  }

  memcpy((void *)&g_wifiFirmwareEntryWord, firmwareData, sizeof(g_wifiFirmwareEntryWord));
  g_wifiFirmwareSize = firmwareSize;
  g_wifiNvramSize = nvramSize;
  g_wifiNvramStagingAddress = (APP_WIFI_43362_RAM_BASE + APP_WIFI_43362_RAM_SIZE - APP_WIFI_NVRAM_TRAILER_SIZE) - roundedNvramSize;

  nvramWordCount = roundedNvramSize / sizeof(uint32_t);
  g_wifiNvramTrailerWord = ((~nvramWordCount) << 16U) | (nvramWordCount & 0x0000FFFFU);

  return HAL_OK;
}

HAL_StatusTypeDef APP_WiFi_Platform_StageFirmwareImage(void)
{
  uint32_t bytesStaged = 0U;

  if (g_wifiFirmwareSize == 0U)
  {
    return HAL_ERROR;
  }

  if (APP_WiFi_Platform_WriteResourceToBackplane(APP_WiFi_Resources_ReadFirmware,
                                                 g_wifiFirmwareSize,
                                                 APP_WIFI_43362_RAM_BASE,
                                                 g_wifiFirmwareSize,
                                                 &bytesStaged) != HAL_OK)
  {
    return HAL_ERROR;
  }

  g_wifiFirmwareBytesStaged = bytesStaged;
  return HAL_OK;
}

HAL_StatusTypeDef APP_WiFi_Platform_StageNvramImage(void)
{
  const uint32_t roundedNvramSize = (g_wifiNvramSize + 3U) & ~0x3U;
  const uint32_t trailerAddress = APP_WIFI_43362_RAM_BASE + APP_WIFI_43362_RAM_SIZE - APP_WIFI_NVRAM_TRAILER_SIZE;
  uint32_t bytesStaged = 0U;

  if ((g_wifiNvramSize == 0U) || (g_wifiNvramStagingAddress == 0U))
  {
    return HAL_ERROR;
  }

  if (APP_WiFi_Platform_WriteResourceToBackplane(APP_WiFi_Resources_ReadNvram,
                                                 g_wifiNvramSize,
                                                 g_wifiNvramStagingAddress,
                                                 roundedNvramSize,
                                                 &bytesStaged) != HAL_OK)
  {
    return HAL_ERROR;
  }

  g_wifiNvramBytesStaged = bytesStaged;

  if (APP_WiFi_Platform_BackplaneWrite32(trailerAddress, g_wifiNvramTrailerWord) != HAL_OK)
  {
    return HAL_ERROR;
  }

  return HAL_OK;
}

HAL_StatusTypeDef APP_WiFi_Platform_ReleaseWlanArmCore(void)
{
  return APP_WiFi_Platform_ResetDeviceCore(APP_WIFI_WLAN_ARM_WRAPPER_BASE, 0U);
}

HAL_StatusTypeDef APP_WiFi_Platform_WaitForFirmwareBoot(void)
{
  uint32_t attempt = 0U;

  for (attempt = 0U; attempt < APP_WIFI_FIRMWARE_BOOT_TIMEOUT_MS; ++attempt)
  {
    if (APP_WiFi_Platform_DeviceCoreIsUp(APP_WIFI_WLAN_ARM_WRAPPER_BASE) == HAL_OK)
    {
      return HAL_OK;
    }

    osDelay(1U);
  }

  g_wifiLastSdioError = SDMMC_ERROR_TIMEOUT;
  return HAL_TIMEOUT;
}

HAL_StatusTypeDef APP_WiFi_Platform_ProbeSharedMemory(void)
{
  APP_WiFi_WlanShared_t shared = {0};
  uint32_t sharedAddress = 0U;

  if (APP_WiFi_Platform_BackplaneRead32(APP_WIFI_WLAN_SHARED_PTR_ADDRESS, &sharedAddress) != HAL_OK)
  {
    return HAL_ERROR;
  }

  g_wifiWlanSharedAddress = sharedAddress;

  if ((sharedAddress == 0U) ||
      ((sharedAddress & (sizeof(uint32_t) - 1U)) != 0U) ||
      (sharedAddress < APP_WIFI_43362_RAM_BASE) ||
      (sharedAddress >= (APP_WIFI_43362_RAM_BASE + APP_WIFI_43362_RAM_SIZE)))
  {
    return HAL_ERROR;
  }

  if (APP_WiFi_Platform_BackplaneRead(sharedAddress, (uint8_t *)&shared, (uint16_t)sizeof(shared)) != HAL_OK)
  {
    return HAL_ERROR;
  }

  g_wifiWlanSharedFlags = shared.flags;
  g_wifiTrapAddress = shared.trapAddress;
  g_wifiAssertExpressionAddress = shared.assertExpressionAddress;
  g_wifiAssertFileAddress = shared.assertFileAddress;
  g_wifiAssertLine = shared.assertLine;
  g_wifiConsoleAddress = shared.consoleAddress;
  g_wifiMsgtraceAddress = shared.msgtraceAddress;
  g_wifiFirmwareId = shared.firmwareId;

  if ((shared.flags & APP_WIFI_WLAN_SHARED_VERSION_MASK) != APP_WIFI_WLAN_SHARED_VERSION)
  {
    return HAL_ERROR;
  }

  return HAL_OK;
}

HAL_StatusTypeDef APP_WiFi_Platform_ProbeConsole(void)
{
  APP_WiFi_HndLog_t logHeader = {0};
  uint32_t logAddress = 0U;

  if (g_wifiConsoleAddress == 0U)
  {
    return HAL_ERROR;
  }

  logAddress = g_wifiConsoleAddress + APP_WIFI_CONSOLE_LOG_OFFSET;
  if (APP_WiFi_Platform_BackplaneRead(logAddress, (uint8_t *)&logHeader, (uint16_t)sizeof(logHeader)) != HAL_OK)
  {
    return HAL_ERROR;
  }

  g_wifiConsoleBufferAddress = logHeader.bufferAddress;
  g_wifiConsoleBufferSize = logHeader.bufferSize;
  g_wifiConsoleWriteIndex = logHeader.writeIndex;
  g_wifiConsoleOutIndex = logHeader.outIndex;

  if ((logHeader.bufferAddress == 0U) ||
      (logHeader.bufferSize == 0U) ||
      (logHeader.bufferAddress < APP_WIFI_43362_RAM_BASE) ||
      (logHeader.bufferAddress >= (APP_WIFI_43362_RAM_BASE + APP_WIFI_43362_RAM_SIZE)) ||
      (logHeader.bufferSize > (APP_WIFI_43362_RAM_BASE + APP_WIFI_43362_RAM_SIZE - logHeader.bufferAddress)) ||
      (logHeader.bufferSize > APP_WIFI_43362_RAM_SIZE) ||
      (logHeader.writeIndex > logHeader.bufferSize) ||
      (logHeader.outIndex > logHeader.bufferSize))
  {
    return HAL_ERROR;
  }

  return HAL_OK;
}

HAL_StatusTypeDef APP_WiFi_Platform_ProbeMailbox(void)
{
  uint32_t interruptStatus = 0U;
  uint32_t mailboxData = 0U;

  if (APP_WiFi_Platform_BackplaneRead32(APP_WIFI_SDIO_INT_STATUS_ADDRESS, &interruptStatus) != HAL_OK)
  {
    return HAL_ERROR;
  }
  g_wifiInterruptStatus = interruptStatus;

  if (APP_WiFi_Platform_BackplaneRead32(APP_WIFI_SDIO_TO_HOST_MAILBOX_DATA_ADDRESS, &mailboxData) != HAL_OK)
  {
    return HAL_ERROR;
  }
  g_wifiHostMailboxData = mailboxData;

  if ((interruptStatus & APP_WIFI_I_HMB_HOST_INT) != 0U)
  {
    if (APP_WiFi_Platform_BackplaneWrite32(APP_WIFI_SDIO_INT_STATUS_ADDRESS, APP_WIFI_I_HMB_HOST_INT) != HAL_OK)
    {
      return HAL_ERROR;
    }

    if (APP_WiFi_Platform_BackplaneRead32(APP_WIFI_SDIO_INT_STATUS_ADDRESS, &interruptStatus) != HAL_OK)
    {
      return HAL_ERROR;
    }
    g_wifiInterruptStatus = interruptStatus;
  }

  return HAL_OK;
}

uint32_t APP_WiFi_Platform_GetSdioInterruptCount(void)
{
  return g_wifiSdioInterruptCount;
}

uint32_t APP_WiFi_Platform_GetLastSdioStatus(void)
{
  return g_wifiLastSdioStatus;
}

uint32_t APP_WiFi_Platform_GetLastSdioError(void)
{
  return g_wifiLastSdioError;
}

uint32_t APP_WiFi_Platform_GetSdioOcr(void)
{
  return g_wifiSdioOcr;
}

uint16_t APP_WiFi_Platform_GetSdioRca(void)
{
  return g_wifiSdioRca;
}

uint32_t APP_WiFi_Platform_GetLastCmd53Word(void)
{
  return g_wifiLastCmd53Word;
}

uint32_t APP_WiFi_Platform_GetLastBackplaneWord(void)
{
  return g_wifiLastBackplaneWord;
}

uint32_t APP_WiFi_Platform_GetBackplaneWindowBase(void)
{
  return g_wifiBackplaneWindowBase;
}

uint8_t APP_WiFi_Platform_GetChipClockCsr(void)
{
  return g_wifiChipClockCsr;
}

uint8_t APP_WiFi_Platform_GetCccrInterruptEnable(void)
{
  return g_wifiCccrInterruptEnable;
}

uint8_t APP_WiFi_Platform_GetSepInterruptControl(void)
{
  return g_wifiSepInterruptControl;
}

uint8_t APP_WiFi_Platform_GetCccrIoEnable(void)
{
  return g_wifiCccrIoEnable;
}

uint8_t APP_WiFi_Platform_GetCccrBusControl(void)
{
  return g_wifiCccrBusControl;
}

uint8_t APP_WiFi_Platform_GetCccrRevision(void)
{
  return g_wifiCccrRevision;
}

uint8_t APP_WiFi_Platform_GetCccrSdRevision(void)
{
  return g_wifiCccrSdRevision;
}

uint8_t APP_WiFi_Platform_GetCccrIoReady(void)
{
  return g_wifiCccrIoReady;
}

uint8_t APP_WiFi_Platform_GetCccrCapabilities(void)
{
  return g_wifiCccrCapabilities;
}

uint32_t APP_WiFi_Platform_GetHostInterruptMask(void)
{
  return g_wifiHostInterruptMask;
}

uint32_t APP_WiFi_Platform_GetFunctionInterruptMask(void)
{
  return g_wifiFunctionInterruptMask;
}

uint32_t APP_WiFi_Platform_GetFirmwareSize(void)
{
  return g_wifiFirmwareSize;
}

uint32_t APP_WiFi_Platform_GetFirmwareEntryWord(void)
{
  return g_wifiFirmwareEntryWord;
}

uint32_t APP_WiFi_Platform_GetNvramSize(void)
{
  return g_wifiNvramSize;
}

uint32_t APP_WiFi_Platform_GetNvramStagingAddress(void)
{
  return g_wifiNvramStagingAddress;
}

uint32_t APP_WiFi_Platform_GetNvramTrailerWord(void)
{
  return g_wifiNvramTrailerWord;
}

uint32_t APP_WiFi_Platform_GetFirmwareBytesStaged(void)
{
  return g_wifiFirmwareBytesStaged;
}

uint32_t APP_WiFi_Platform_GetNvramBytesStaged(void)
{
  return g_wifiNvramBytesStaged;
}

uint8_t APP_WiFi_Platform_GetWlanCoreIoCtrl(void)
{
  return g_wifiWlanCoreIoCtrl;
}

uint8_t APP_WiFi_Platform_GetWlanCoreResetCtrl(void)
{
  return g_wifiWlanCoreResetCtrl;
}

uint32_t APP_WiFi_Platform_GetWlanSharedAddress(void)
{
  return g_wifiWlanSharedAddress;
}

uint32_t APP_WiFi_Platform_GetWlanSharedFlags(void)
{
  return g_wifiWlanSharedFlags;
}

uint32_t APP_WiFi_Platform_GetTrapAddress(void)
{
  return g_wifiTrapAddress;
}

uint32_t APP_WiFi_Platform_GetAssertExpressionAddress(void)
{
  return g_wifiAssertExpressionAddress;
}

uint32_t APP_WiFi_Platform_GetAssertFileAddress(void)
{
  return g_wifiAssertFileAddress;
}

uint32_t APP_WiFi_Platform_GetAssertLine(void)
{
  return g_wifiAssertLine;
}

uint32_t APP_WiFi_Platform_GetConsoleAddress(void)
{
  return g_wifiConsoleAddress;
}

uint32_t APP_WiFi_Platform_GetMsgtraceAddress(void)
{
  return g_wifiMsgtraceAddress;
}

uint32_t APP_WiFi_Platform_GetFirmwareId(void)
{
  return g_wifiFirmwareId;
}

uint32_t APP_WiFi_Platform_GetConsoleBufferAddress(void)
{
  return g_wifiConsoleBufferAddress;
}

uint32_t APP_WiFi_Platform_GetConsoleBufferSize(void)
{
  return g_wifiConsoleBufferSize;
}

uint32_t APP_WiFi_Platform_GetConsoleWriteIndex(void)
{
  return g_wifiConsoleWriteIndex;
}

uint32_t APP_WiFi_Platform_GetConsoleOutIndex(void)
{
  return g_wifiConsoleOutIndex;
}

uint32_t APP_WiFi_Platform_GetInterruptStatus(void)
{
  return g_wifiInterruptStatus;
}

uint32_t APP_WiFi_Platform_GetHostMailboxData(void)
{
  return g_wifiHostMailboxData;
}

void APP_WiFi_Platform_SDMMC_IRQHandler(void)
{
  const uint32_t status = SDMMC1->STA;

  g_wifiSdioInterruptCount++;
  g_wifiLastSdioStatus = status;

  /*
   * AP6181 uses the SDMMC peripheral as a raw SDIO host, not as a standard
   * SD card. Until the WWD bus layer is wired in, we keep the handler owned by
   * the Wi-Fi adaptation layer and just clear pending flags to avoid an
   * interrupt storm.
   */
  if (status != 0U)
  {
    SDMMC1->ICR = status;
  }
  else
  {
    SDMMC1->ICR = 0xFFFFFFFFU;
  }
}

static HAL_StatusTypeDef APP_WiFi_Platform_SdioErrorToHalStatus(uint32_t error)
{
  if (error == SDMMC_ERROR_NONE)
  {
    return HAL_OK;
  }

  if (error == SDMMC_ERROR_TIMEOUT)
  {
    return HAL_TIMEOUT;
  }

  return HAL_ERROR;
}

static HAL_StatusTypeDef APP_WiFi_Platform_ApplyHostBusWidth(uint32_t busWide)
{
  SDMMC_InitTypeDef sdioInit = {0};

  /*
   * Keep the host configuration minimal and deterministic:
   * start in slow 1-bit mode, then reuse the same settings when switching
   * to 4-bit after the card-side CCCR/BUS interface control is updated.
   */
  sdioInit.ClockDiv = SDMMC_INIT_CLK_DIV;
  sdioInit.ClockEdge = SDMMC_CLOCK_EDGE_RISING;
  sdioInit.ClockPowerSave = SDMMC_CLOCK_POWER_SAVE_DISABLE;
  sdioInit.BusWide = busWide;
  sdioInit.HardwareFlowControl = SDMMC_HARDWARE_FLOW_CONTROL_DISABLE;

  if (SDMMC_Init(SDMMC1, sdioInit) != HAL_OK)
  {
    return HAL_ERROR;
  }

  if (SDMMC_PowerState_ON(SDMMC1) != HAL_OK)
  {
    return HAL_ERROR;
  }

  if (SDMMC_SetSDMMCReadWaitMode(SDMMC1, SDMMC_READ_WAIT_MODE_CLK) != HAL_OK)
  {
    return HAL_ERROR;
  }

  g_wifiLastSdioStatus = SDMMC1->STA;
  g_wifiLastSdioError = SDMMC_ERROR_NONE;
  return HAL_OK;
}

static HAL_StatusTypeDef APP_WiFi_Platform_WaitCmd53Transfer(void)
{
  const uint32_t deadline = HAL_GetTick() + APP_WIFI_SDIO_CMD53_TIMEOUT_MS;

  for (;;)
  {
    const uint32_t status = SDMMC1->STA;

    g_wifiLastSdioStatus = status;

    if ((status & (SDMMC_FLAG_DCRCFAIL | SDMMC_FLAG_DTIMEOUT | SDMMC_FLAG_RXOVERR | SDMMC_FLAG_TXUNDERR | SDMMC_FLAG_IDMATE)) != 0U)
    {
      g_wifiLastSdioError = SDMMC_ERROR_GENERAL_UNKNOWN_ERR;
      SDMMC1->ICR = 0xFFFFFFFFU;
      SDMMC1->DLEN = 0U;
      SDMMC1->DCTRL = SDMMC_DCTRL_SDIOEN;
      SDMMC1->IDMACTRL = SDMMC_DISABLE_IDMA;
      SDMMC1->CMD = 0U;
      return HAL_ERROR;
    }

    if ((status & SDMMC_FLAG_DATAEND) != 0U)
    {
      g_wifiLastSdioError = SDMMC_ERROR_NONE;
      SDMMC1->ICR = 0xFFFFFFFFU;
      SDMMC1->DLEN = 0U;
      SDMMC1->DCTRL = SDMMC_DCTRL_SDIOEN;
      SDMMC1->IDMACTRL = SDMMC_DISABLE_IDMA;
      SDMMC1->CMD = 0U;
      return HAL_OK;
    }

    if ((int32_t)(HAL_GetTick() - deadline) >= 0)
    {
      g_wifiLastSdioError = SDMMC_ERROR_TIMEOUT;
      SDMMC1->ICR = 0xFFFFFFFFU;
      SDMMC1->DLEN = 0U;
      SDMMC1->DCTRL = SDMMC_DCTRL_SDIOEN;
      SDMMC1->IDMACTRL = SDMMC_DISABLE_IDMA;
      SDMMC1->CMD = 0U;
      return HAL_TIMEOUT;
    }
  }
}

static HAL_StatusTypeDef APP_WiFi_Platform_Cmd53Write(uint8_t functionNumber, uint32_t address, const uint8_t *data, uint16_t dataLength)
{
  const uint32_t transferLength = APP_WiFi_Platform_RoundTransferLength(dataLength);
  const uint32_t dctrlBlockSize = APP_WiFi_Platform_GetDctrlBlockSize(transferLength);
  const uint32_t argument = APP_WiFi_Platform_BuildCmd53Argument(1U,
                                                                 functionNumber,
                                                                 address,
                                                                 0U,
                                                                 1U,
                                                                 (uint16_t)transferLength);

  if ((data == NULL) || (dataLength == 0U) || (transferLength == 0U) || (dctrlBlockSize == 0U) ||
      (transferLength > sizeof(g_wifiCmd53Scratch)))
  {
    return HAL_ERROR;
  }

  memset((void *)g_wifiCmd53Scratch, 0, sizeof(g_wifiCmd53Scratch));
  memcpy((void *)g_wifiCmd53Scratch, data, dataLength);

  SDMMC1->MASK = 0U;
  SDMMC1->ICR = 0xFFFFFFFFU;
  SDMMC1->DTIMER = SDMMC_DATATIMEOUT;
  SDMMC1->DLEN = transferLength;
  SDMMC1->DCTRL = dctrlBlockSize | SDMMC_TRANSFER_DIR_TO_CARD | SDMMC_TRANSFER_MODE_BLOCK | SDMMC_DCTRL_SDIOEN;
  SDMMC1->IDMACTRL = SDMMC_ENABLE_IDMA_SINGLE_BUFF;
  SDMMC1->IDMABASE0 = (uint32_t)g_wifiCmd53Scratch;

  SDMMC1->ARG = argument;
  SDMMC1->CMD = (uint32_t)(SDMMC_CMD_SDMMC_RW_EXTENDED | SDMMC_RESPONSE_SHORT | SDMMC_WAIT_NO | SDMMC_CPSM_ENABLE | SDMMC_CMD_CMDTRANS);

  if (APP_WiFi_Platform_WaitCmd53Transfer() != HAL_OK)
  {
    return HAL_ERROR;
  }

  return HAL_OK;
}

static HAL_StatusTypeDef APP_WiFi_Platform_BackplaneRead8(uint32_t address, uint8_t *value)
{
  return APP_WiFi_Platform_BackplaneRead(address, value, 1U);
}

static HAL_StatusTypeDef APP_WiFi_Platform_BackplaneWrite8(uint32_t address, uint8_t value)
{
  return APP_WiFi_Platform_BackplaneWrite(address, &value, 1U);
}

static HAL_StatusTypeDef APP_WiFi_Platform_ReadCoreState(uint32_t wrapperBase, uint8_t *ioCtrl, uint8_t *resetCtrl)
{
  uint8_t localIoCtrl = 0U;
  uint8_t localResetCtrl = 0U;

  if ((ioCtrl == NULL) || (resetCtrl == NULL))
  {
    return HAL_ERROR;
  }

  if (APP_WiFi_Platform_BackplaneRead8(wrapperBase + APP_WIFI_AI_IOCTRL_OFFSET, &localIoCtrl) != HAL_OK)
  {
    return HAL_ERROR;
  }

  if (APP_WiFi_Platform_BackplaneRead8(wrapperBase + APP_WIFI_AI_RESETCTRL_OFFSET, &localResetCtrl) != HAL_OK)
  {
    return HAL_ERROR;
  }

  *ioCtrl = localIoCtrl;
  *resetCtrl = localResetCtrl;

  if (wrapperBase == APP_WIFI_WLAN_ARM_WRAPPER_BASE)
  {
    g_wifiWlanCoreIoCtrl = localIoCtrl;
    g_wifiWlanCoreResetCtrl = localResetCtrl;
  }

  return HAL_OK;
}

static HAL_StatusTypeDef APP_WiFi_Platform_DisableDeviceCore(uint32_t wrapperBase, uint8_t cpuHalt)
{
  uint8_t resetCtrl = 0U;
  uint8_t ioCtrl = 0U;

  if (APP_WiFi_Platform_BackplaneRead8(wrapperBase + APP_WIFI_AI_RESETCTRL_OFFSET, &resetCtrl) != HAL_OK)
  {
    return HAL_ERROR;
  }

  if ((resetCtrl & APP_WIFI_AIRC_RESET) != 0U)
  {
    return HAL_OK;
  }

  ioCtrl = (cpuHalt != 0U) ? APP_WIFI_SICF_CPUHALT : 0U;
  if (APP_WiFi_Platform_BackplaneWrite8(wrapperBase + APP_WIFI_AI_IOCTRL_OFFSET, ioCtrl) != HAL_OK)
  {
    return HAL_ERROR;
  }

  if (APP_WiFi_Platform_BackplaneRead8(wrapperBase + APP_WIFI_AI_IOCTRL_OFFSET, &ioCtrl) != HAL_OK)
  {
    return HAL_ERROR;
  }

  osDelay(1U);

  if (APP_WiFi_Platform_BackplaneWrite8(wrapperBase + APP_WIFI_AI_RESETCTRL_OFFSET, APP_WIFI_AIRC_RESET) != HAL_OK)
  {
    return HAL_ERROR;
  }

  osDelay(1U);
  return APP_WiFi_Platform_ReadCoreState(wrapperBase, &ioCtrl, &resetCtrl);
}

static HAL_StatusTypeDef APP_WiFi_Platform_ResetDeviceCore(uint32_t wrapperBase, uint8_t cpuHalt)
{
  uint8_t ioCtrl = 0U;
  uint8_t resetCtrl = 0U;
  const uint8_t haltBits = (cpuHalt != 0U) ? APP_WIFI_SICF_CPUHALT : 0U;

  if (APP_WiFi_Platform_DisableDeviceCore(wrapperBase, cpuHalt) != HAL_OK)
  {
    return HAL_ERROR;
  }

  if (APP_WiFi_Platform_BackplaneWrite8(wrapperBase + APP_WIFI_AI_IOCTRL_OFFSET,
                                        (uint8_t)(APP_WIFI_SICF_FGC | APP_WIFI_SICF_CLOCK_EN | haltBits)) != HAL_OK)
  {
    return HAL_ERROR;
  }

  if (APP_WiFi_Platform_BackplaneRead8(wrapperBase + APP_WIFI_AI_IOCTRL_OFFSET, &ioCtrl) != HAL_OK)
  {
    return HAL_ERROR;
  }

  if (APP_WiFi_Platform_BackplaneWrite8(wrapperBase + APP_WIFI_AI_RESETCTRL_OFFSET, 0U) != HAL_OK)
  {
    return HAL_ERROR;
  }

  osDelay(1U);

  if (APP_WiFi_Platform_BackplaneWrite8(wrapperBase + APP_WIFI_AI_IOCTRL_OFFSET,
                                        (uint8_t)(APP_WIFI_SICF_CLOCK_EN | haltBits)) != HAL_OK)
  {
    return HAL_ERROR;
  }

  if (APP_WiFi_Platform_BackplaneRead8(wrapperBase + APP_WIFI_AI_IOCTRL_OFFSET, &ioCtrl) != HAL_OK)
  {
    return HAL_ERROR;
  }

  osDelay(1U);
  return APP_WiFi_Platform_ReadCoreState(wrapperBase, &ioCtrl, &resetCtrl);
}

static HAL_StatusTypeDef APP_WiFi_Platform_DeviceCoreIsUp(uint32_t wrapperBase)
{
  uint8_t ioCtrl = 0U;
  uint8_t resetCtrl = 0U;

  if (APP_WiFi_Platform_ReadCoreState(wrapperBase, &ioCtrl, &resetCtrl) != HAL_OK)
  {
    return HAL_ERROR;
  }

  if ((ioCtrl & (APP_WIFI_SICF_FGC | APP_WIFI_SICF_CLOCK_EN)) != APP_WIFI_SICF_CLOCK_EN)
  {
    return HAL_ERROR;
  }

  if ((resetCtrl & APP_WIFI_AIRC_RESET) != 0U)
  {
    return HAL_ERROR;
  }

  return HAL_OK;
}

static HAL_StatusTypeDef APP_WiFi_Platform_WriteResourceToBackplane(APP_WiFi_ResourceReadFunc_t readFunc,
                                                                    uint32_t resourceSize,
                                                                    uint32_t destinationAddress,
                                                                    uint32_t paddedSize,
                                                                    uint32_t *bytesStaged)
{
  uint32_t offset = 0U;

  if ((readFunc == NULL) || (resourceSize == 0U) || (paddedSize < resourceSize) || (bytesStaged == NULL))
  {
    return HAL_ERROR;
  }

  *bytesStaged = 0U;

  while (offset < paddedSize)
  {
    const uint32_t chunkAddress = destinationAddress + offset;
    const uint32_t windowRemaining = APP_WIFI_BACKPLANE_WINDOW_SIZE - (chunkAddress & APP_WIFI_BACKPLANE_ADDRESS_MASK);
    uint32_t chunkSize = paddedSize - offset;
    uint32_t sizeRead = 0U;

    if (chunkSize > APP_WIFI_STAGE_CHUNK_SIZE)
    {
      chunkSize = APP_WIFI_STAGE_CHUNK_SIZE;
    }

    if (chunkSize > windowRemaining)
    {
      chunkSize = windowRemaining;
    }

    memset(g_wifiStageBuffer, 0, chunkSize);

    if (offset < resourceSize)
    {
      const uint32_t readableSize = resourceSize - offset;
      const uint32_t requestedSize = (chunkSize < readableSize) ? chunkSize : readableSize;

      if (readFunc(offset, g_wifiStageBuffer, requestedSize, &sizeRead) != HAL_OK)
      {
        return HAL_ERROR;
      }

      if (sizeRead != requestedSize)
      {
        return HAL_ERROR;
      }
    }

    if (APP_WiFi_Platform_BackplaneWrite(chunkAddress, g_wifiStageBuffer, (uint16_t)chunkSize) != HAL_OK)
    {
      return HAL_ERROR;
    }

    offset += chunkSize;
    *bytesStaged = offset;
  }

  return HAL_OK;
}

static uint32_t APP_WiFi_Platform_BuildCmd53Argument(uint8_t rwFlag,
                                                     uint8_t functionNumber,
                                                     uint32_t address,
                                                     uint8_t blockMode,
                                                     uint8_t opCode,
                                                     uint16_t count)
{
  uint32_t argument = 0U;

  argument |= ((uint32_t)count & 0x1FFU);
  argument |= ((address & 0x1FFFFU) << 9U);
  argument |= (((uint32_t)opCode & 0x1U) << 26U);
  argument |= (((uint32_t)blockMode & 0x1U) << 27U);
  argument |= (((uint32_t)functionNumber & 0x7U) << 28U);
  argument |= (((uint32_t)rwFlag & 0x1U) << 31U);

  return argument;
}

static uint32_t APP_WiFi_Platform_BuildCmd52Argument(uint8_t rwFlag,
                                                     uint8_t functionNumber,
                                                     uint32_t address,
                                                     uint8_t rawFlag,
                                                     uint8_t value)
{
  uint32_t argument = 0U;

  argument |= (uint32_t)value;
  argument |= ((address & 0x1FFFFU) << 9U);
  argument |= (((uint32_t)rawFlag & 0x1U) << 27U);
  argument |= (((uint32_t)functionNumber & 0x7U) << 28U);
  argument |= (((uint32_t)rwFlag & 0x1U) << 31U);

  return argument;
}

static uint32_t APP_WiFi_Platform_RoundTransferLength(uint16_t dataLength)
{
  uint32_t transferLength = 1U;

  while (transferLength < dataLength)
  {
    transferLength <<= 1U;
  }

  if (transferLength > APP_WIFI_SDIO_BLOCK_SIZE)
  {
    return 0U;
  }

  return transferLength;
}

static uint32_t APP_WiFi_Platform_GetDctrlBlockSize(uint32_t transferLength)
{
  switch (transferLength)
  {
    case 1U:
      return SDMMC_DATABLOCK_SIZE_1B;
    case 2U:
      return SDMMC_DATABLOCK_SIZE_2B;
    case 4U:
      return SDMMC_DATABLOCK_SIZE_4B;
    case 8U:
      return SDMMC_DATABLOCK_SIZE_8B;
    case 16U:
      return SDMMC_DATABLOCK_SIZE_16B;
    case 32U:
      return SDMMC_DATABLOCK_SIZE_32B;
    case 64U:
      return SDMMC_DATABLOCK_SIZE_64B;
    default:
      return 0U;
  }
}
