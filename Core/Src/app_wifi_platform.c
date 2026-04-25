#include "app_wifi_platform.h"

#include "cmsis_os2.h"
#include "main.h"
#include "sdmmc.h"
#include "stm32h7xx_ll_sdmmc.h"

#define APP_WIFI_SDIO_ENUM_TIMEOUT_MS 500U
#define APP_WIFI_SDIO_FN1_ENABLE_TIMEOUT_MS 500U
#define APP_WIFI_SDIO_FN0             0U
#define APP_WIFI_SDIO_CCCR_REV        0x00U
#define APP_WIFI_SDIO_CCCR_SDREV      0x01U
#define APP_WIFI_SDIO_CCCR_IOEN       0x02U
#define APP_WIFI_SDIO_CCCR_IORDY      0x03U
#define APP_WIFI_SDIO_CCCR_BICTRL     0x07U
#define APP_WIFI_SDIO_CCCR_CAPS       0x08U
#define APP_WIFI_SDIO_CCCR_BLKSIZE_0  0x10U
#define APP_WIFI_SDIO_CCCR_BLKSIZE_1  0x11U
#define APP_WIFI_SDIO_CCCR_F1BLKSIZE_0 0x110U
#define APP_WIFI_SDIO_CCCR_F1BLKSIZE_1 0x111U
#define APP_WIFI_SDIO_BLOCK_SIZE      64U
#define APP_WIFI_SDIO_FUNC_ENABLE_1   0x02U
#define APP_WIFI_SDIO_FUNC_READY_1    0x02U
#define APP_WIFI_SDIO_BUS_WIDTH_MASK  0x03U
#define APP_WIFI_SDIO_BUS_WIDTH_4BIT  0x02U

static volatile uint32_t g_wifiSdioInterruptCount = 0U;
static volatile uint32_t g_wifiLastSdioStatus = 0U;
static volatile uint32_t g_wifiLastSdioError = SDMMC_ERROR_NONE;
static volatile uint32_t g_wifiSdioOcr = 0U;
static volatile uint16_t g_wifiSdioRca = 0U;
static volatile uint32_t g_wifiSdioHostInitialized = 0U;
static volatile uint32_t g_wifiSdioEnumerated = 0U;
static volatile uint32_t g_wifiSdioBusConfigured = 0U;
static volatile uint8_t g_wifiCccrIoEnable = 0U;
static volatile uint8_t g_wifiCccrBusControl = 0U;
static volatile uint8_t g_wifiCccrRevision = 0U;
static volatile uint8_t g_wifiCccrSdRevision = 0U;
static volatile uint8_t g_wifiCccrIoReady = 0U;
static volatile uint8_t g_wifiCccrCapabilities = 0U;

static HAL_StatusTypeDef APP_WiFi_Platform_SdioErrorToHalStatus(uint32_t error);
static HAL_StatusTypeDef APP_WiFi_Platform_ApplyHostBusWidth(uint32_t busWide);
static uint32_t APP_WiFi_Platform_BuildCmd52Argument(uint8_t rwFlag,
                                                     uint8_t functionNumber,
                                                     uint32_t address,
                                                     uint8_t rawFlag,
                                                     uint8_t value);

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
