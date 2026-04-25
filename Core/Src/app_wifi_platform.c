#include "app_wifi_platform.h"

#include "cmsis_os2.h"
#include "main.h"
#include "sdmmc.h"
#include "stm32h7xx_ll_sdmmc.h"

#define APP_WIFI_SDIO_ENUM_TIMEOUT_MS 500U

static volatile uint32_t g_wifiSdioInterruptCount = 0U;
static volatile uint32_t g_wifiLastSdioStatus = 0U;
static volatile uint32_t g_wifiLastSdioError = SDMMC_ERROR_NONE;
static volatile uint32_t g_wifiSdioOcr = 0U;
static volatile uint16_t g_wifiSdioRca = 0U;
static volatile uint32_t g_wifiSdioHostInitialized = 0U;
static volatile uint32_t g_wifiSdioEnumerated = 0U;

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
  HAL_GPIO_WritePin(WIFI_RESET_GPIO_Port, WIFI_RESET_Pin, GPIO_PIN_SET);
}

void APP_WiFi_Platform_SetResetPin(GPIO_PinState pinState)
{
  HAL_GPIO_WritePin(WIFI_RESET_GPIO_Port, WIFI_RESET_Pin, pinState);
}

HAL_StatusTypeDef APP_WiFi_Platform_SdioHostInit(void)
{
  SDMMC_InitTypeDef sdioInit = {0};

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

  /*
   * Match the official AP6181 bring-up strategy:
   * start from a very slow 1-bit SDIO clock, then move to wider/faster modes
   * only after the device is enumerated by the future WWD bus layer.
   */
  sdioInit.ClockDiv = SDMMC_INIT_CLK_DIV;
  sdioInit.ClockEdge = SDMMC_CLOCK_EDGE_RISING;
  sdioInit.ClockPowerSave = SDMMC_CLOCK_POWER_SAVE_DISABLE;
  sdioInit.BusWide = SDMMC_BUS_WIDE_1B;
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
