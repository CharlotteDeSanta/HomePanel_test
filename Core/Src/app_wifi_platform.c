#include "app_wifi_platform.h"

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
static volatile uint8_t g_wifiChipClockCsr = 0U;
static volatile uint8_t g_wifiCccrInterruptEnable = 0U;
static volatile uint8_t g_wifiSepInterruptControl = 0U;
static volatile uint8_t g_wifiCccrIoEnable = 0U;
static volatile uint8_t g_wifiCccrBusControl = 0U;
static volatile uint8_t g_wifiCccrRevision = 0U;
static volatile uint8_t g_wifiCccrSdRevision = 0U;
static volatile uint8_t g_wifiCccrIoReady = 0U;
static volatile uint8_t g_wifiCccrCapabilities = 0U;
static uint32_t g_wifiCmd53Scratch[(APP_WIFI_SDIO_BLOCK_SIZE + sizeof(uint32_t) - 1U) / sizeof(uint32_t)] = {0};

static HAL_StatusTypeDef APP_WiFi_Platform_SdioErrorToHalStatus(uint32_t error);
static HAL_StatusTypeDef APP_WiFi_Platform_ApplyHostBusWidth(uint32_t busWide);
static HAL_StatusTypeDef APP_WiFi_Platform_WaitCmd53Transfer(void);
static HAL_StatusTypeDef APP_WiFi_Platform_Cmd53Write(uint8_t functionNumber, uint32_t address, const uint8_t *data, uint16_t dataLength);
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
  g_wifiChipClockCsr = 0U;
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

  if ((data == NULL) || (dataLength == 0U) || (transferLength == 0U) || (dctrlBlockSize == 0U))
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

  if ((data == NULL) || (dataLength == 0U) || (transferLength == 0U) || (dctrlBlockSize == 0U))
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
