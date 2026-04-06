#include "bsp_sdram.h"

#define SDRAM_MODEREG_BURST_LENGTH_1          ((uint16_t)0x0000)
#define SDRAM_MODEREG_BURST_TYPE_SEQUENTIAL   ((uint16_t)0x0000)
#define SDRAM_MODEREG_CAS_LATENCY_3           ((uint16_t)0x0030)
#define SDRAM_MODEREG_OPERATING_MODE_STANDARD ((uint16_t)0x0000)
#define SDRAM_MODEREG_WRITEBURST_MODE_SINGLE  ((uint16_t)0x0200)

SDRAM_HandleTypeDef hsdram1;

static uint32_t bsp_sdram_initialized = 0U;
static uint32_t bsp_fmc_msp_initialized = 0U;

static void BSP_SDRAM_InitializationSequence(void);

void MX_FMC_Init(void)
{
  BSP_SDRAM_Init();
}

void BSP_SDRAM_Init(void)
{
  FMC_SDRAM_TimingTypeDef SdramTiming = {0};

  if (bsp_sdram_initialized != 0U)
  {
    return;
  }

  hsdram1.Instance = FMC_SDRAM_DEVICE;
  hsdram1.Init.SDBank = FMC_SDRAM_BANK2;
  hsdram1.Init.ColumnBitsNumber = FMC_SDRAM_COLUMN_BITS_NUM_9;
  hsdram1.Init.RowBitsNumber = FMC_SDRAM_ROW_BITS_NUM_13;
  hsdram1.Init.MemoryDataWidth = FMC_SDRAM_MEM_BUS_WIDTH_32;
  hsdram1.Init.InternalBankNumber = FMC_SDRAM_INTERN_BANKS_NUM_4;
  hsdram1.Init.CASLatency = FMC_SDRAM_CAS_LATENCY_3;
  hsdram1.Init.WriteProtection = FMC_SDRAM_WRITE_PROTECTION_DISABLE;
  hsdram1.Init.SDClockPeriod = FMC_SDRAM_CLOCK_PERIOD_2;
  hsdram1.Init.ReadBurst = FMC_SDRAM_RBURST_ENABLE;
  hsdram1.Init.ReadPipeDelay = FMC_SDRAM_RPIPE_DELAY_1;

  SdramTiming.LoadToActiveDelay = 2;
  SdramTiming.ExitSelfRefreshDelay = 8;
  SdramTiming.SelfRefreshTime = 5;
  SdramTiming.RowCycleDelay = 8;
  SdramTiming.WriteRecoveryTime = 4;
  SdramTiming.RPDelay = 2;
  SdramTiming.RCDDelay = 2;

  if (HAL_SDRAM_Init(&hsdram1, &SdramTiming) != HAL_OK)
  {
    Error_Handler();
  }

  BSP_SDRAM_InitializationSequence();
  bsp_sdram_initialized = 1U;
}

void BSP_SDRAM_Test(void)
{
  volatile uint32_t *sdram = (uint32_t *)BSP_SDRAM_BASE_ADDR;
  static const uint32_t patterns[] = {
    0x00000000U,
    0xFFFFFFFFU,
    0x55555555U,
    0xAAAAAAAAU,
    0x55AA55AAU
  };

  for (uint32_t p = 0; p < (sizeof(patterns) / sizeof(patterns[0])); p++)
  {
    for (uint32_t i = 0; i < 4096U; i++)
    {
      sdram[i] = patterns[p];
    }

    for (uint32_t i = 0; i < 4096U; i++)
    {
      if (sdram[i] != patterns[p])
      {
        Error_Handler();
      }
    }
  }
}

void BSP_SDRAM_MspInit(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

  if (bsp_fmc_msp_initialized != 0U)
  {
    return;
  }

  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOG_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOI_CLK_ENABLE();

  PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_FMC;
  PeriphClkInitStruct.PLL2.PLL2M = 5;
  PeriphClkInitStruct.PLL2.PLL2N = 144;
  PeriphClkInitStruct.PLL2.PLL2P = 2;
  PeriphClkInitStruct.PLL2.PLL2Q = 2;
  PeriphClkInitStruct.PLL2.PLL2R = 3;
  PeriphClkInitStruct.PLL2.PLL2RGE = RCC_PLL2VCIRANGE_2;
  PeriphClkInitStruct.PLL2.PLL2VCOSEL = RCC_PLL2VCOWIDE;
  PeriphClkInitStruct.PLL2.PLL2FRACN = 0;
  PeriphClkInitStruct.FmcClockSelection = RCC_FMCCLKSOURCE_PLL2;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  __HAL_RCC_FMC_CLK_ENABLE();

  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF12_FMC;

  GPIO_InitStruct.Pin = GPIO_PIN_15 | GPIO_PIN_8 | GPIO_PIN_5 | GPIO_PIN_4
                      | GPIO_PIN_2 | GPIO_PIN_0 | GPIO_PIN_1;
  HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_15 | GPIO_PIN_14
                      | GPIO_PIN_10 | GPIO_PIN_9 | GPIO_PIN_8;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = GPIO_PIN_2 | GPIO_PIN_1 | GPIO_PIN_0 | GPIO_PIN_3
                      | GPIO_PIN_5 | GPIO_PIN_4 | GPIO_PIN_13 | GPIO_PIN_14
                      | GPIO_PIN_12 | GPIO_PIN_15 | GPIO_PIN_11;
  HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = GPIO_PIN_0;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_10 | GPIO_PIN_9
                      | GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_15 | GPIO_PIN_8
                      | GPIO_PIN_13 | GPIO_PIN_7 | GPIO_PIN_14;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11
                      | GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15
                      | GPIO_PIN_6 | GPIO_PIN_7;
  HAL_GPIO_Init(GPIOH, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3
                      | GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7
                      | GPIO_PIN_9 | GPIO_PIN_10;
  HAL_GPIO_Init(GPIOI, &GPIO_InitStruct);

  bsp_fmc_msp_initialized = 1U;
}

void BSP_SDRAM_MspDeInit(void)
{
  if (bsp_fmc_msp_initialized == 0U)
  {
    return;
  }

  __HAL_RCC_FMC_CLK_DISABLE();

  HAL_GPIO_DeInit(GPIOG, GPIO_PIN_15 | GPIO_PIN_8 | GPIO_PIN_5 | GPIO_PIN_4
                        | GPIO_PIN_2 | GPIO_PIN_0 | GPIO_PIN_1);
  HAL_GPIO_DeInit(GPIOD, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_15 | GPIO_PIN_14
                        | GPIO_PIN_10 | GPIO_PIN_9 | GPIO_PIN_8);
  HAL_GPIO_DeInit(GPIOF, GPIO_PIN_2 | GPIO_PIN_1 | GPIO_PIN_0 | GPIO_PIN_3
                        | GPIO_PIN_5 | GPIO_PIN_4 | GPIO_PIN_13 | GPIO_PIN_14
                        | GPIO_PIN_12 | GPIO_PIN_15 | GPIO_PIN_11);
  HAL_GPIO_DeInit(GPIOC, GPIO_PIN_0);
  HAL_GPIO_DeInit(GPIOE, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_10 | GPIO_PIN_9
                        | GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_15 | GPIO_PIN_8
                        | GPIO_PIN_13 | GPIO_PIN_7 | GPIO_PIN_14);
  HAL_GPIO_DeInit(GPIOH, GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11
                        | GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15
                        | GPIO_PIN_6 | GPIO_PIN_7);
  HAL_GPIO_DeInit(GPIOI, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3
                        | GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7
                        | GPIO_PIN_9 | GPIO_PIN_10);

  bsp_fmc_msp_initialized = 0U;
}

void HAL_SDRAM_MspInit(SDRAM_HandleTypeDef *sdramHandle)
{
  if (sdramHandle->Instance == FMC_SDRAM_DEVICE)
  {
    BSP_SDRAM_MspInit();
  }
}

void HAL_SDRAM_MspDeInit(SDRAM_HandleTypeDef *sdramHandle)
{
  if (sdramHandle->Instance == FMC_SDRAM_DEVICE)
  {
    BSP_SDRAM_MspDeInit();
  }
}

static void BSP_SDRAM_InitializationSequence(void)
{
  FMC_SDRAM_CommandTypeDef command = {0};
  uint32_t mode_reg = 0U;

  command.CommandMode = FMC_SDRAM_CMD_CLK_ENABLE;
  command.CommandTarget = FMC_SDRAM_CMD_TARGET_BANK2;
  command.AutoRefreshNumber = 1;
  command.ModeRegisterDefinition = 0;
  if (HAL_SDRAM_SendCommand(&hsdram1, &command, HAL_MAX_DELAY) != HAL_OK)
  {
    Error_Handler();
  }

  HAL_Delay(1);

  command.CommandMode = FMC_SDRAM_CMD_PALL;
  if (HAL_SDRAM_SendCommand(&hsdram1, &command, HAL_MAX_DELAY) != HAL_OK)
  {
    Error_Handler();
  }

  command.CommandMode = FMC_SDRAM_CMD_AUTOREFRESH_MODE;
  command.AutoRefreshNumber = 8;
  if (HAL_SDRAM_SendCommand(&hsdram1, &command, HAL_MAX_DELAY) != HAL_OK)
  {
    Error_Handler();
  }

  mode_reg = SDRAM_MODEREG_BURST_LENGTH_1
           | SDRAM_MODEREG_BURST_TYPE_SEQUENTIAL
           | SDRAM_MODEREG_CAS_LATENCY_3
           | SDRAM_MODEREG_OPERATING_MODE_STANDARD
           | SDRAM_MODEREG_WRITEBURST_MODE_SINGLE;

  command.CommandMode = FMC_SDRAM_CMD_LOAD_MODE;
  command.AutoRefreshNumber = 1;
  command.ModeRegisterDefinition = mode_reg;
  if (HAL_SDRAM_SendCommand(&hsdram1, &command, HAL_MAX_DELAY) != HAL_OK)
  {
    Error_Handler();
  }

  if (HAL_SDRAM_ProgramRefreshRate(&hsdram1, 824) != HAL_OK)
  {
    Error_Handler();
  }
}
