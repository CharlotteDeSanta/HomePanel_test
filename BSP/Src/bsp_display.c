#include "bsp_display.h"

LTDC_HandleTypeDef hltdc;

static uint32_t bsp_ltdc_initialized = 0U;
static uint32_t bsp_ltdc_msp_initialized = 0U;

static uint32_t BSP_Display_GetColorBarIndex(uint32_t x);

void MX_LTDC_Init(void)
{
  BSP_Display_Init();
}

void BSP_Display_Init(void)
{
  LTDC_LayerCfgTypeDef pLayerCfg = {0};

  if (bsp_ltdc_initialized != 0U)
  {
    return;
  }

  hltdc.Instance = LTDC;
  hltdc.Init.HSPolarity = LTDC_HSPOLARITY_AL;
  hltdc.Init.VSPolarity = LTDC_VSPOLARITY_AL;
  hltdc.Init.DEPolarity = LTDC_DEPOLARITY_AL;
  hltdc.Init.PCPolarity = LTDC_PCPOLARITY_IPC;
  hltdc.Init.HorizontalSync = 0;
  hltdc.Init.VerticalSync = 0;
  hltdc.Init.AccumulatedHBP = 46;
  hltdc.Init.AccumulatedVBP = 23;
  hltdc.Init.AccumulatedActiveW = 846;
  hltdc.Init.AccumulatedActiveH = 503;
  hltdc.Init.TotalWidth = 868;
  hltdc.Init.TotalHeigh = 525;
  hltdc.Init.Backcolor.Blue = 0;
  hltdc.Init.Backcolor.Green = 0;
  hltdc.Init.Backcolor.Red = 0;
  if (HAL_LTDC_Init(&hltdc) != HAL_OK)
  {
    Error_Handler();
  }

  pLayerCfg.WindowX0 = 0;
  pLayerCfg.WindowX1 = BSP_DISPLAY_WIDTH;
  pLayerCfg.WindowY0 = 0;
  pLayerCfg.WindowY1 = BSP_DISPLAY_HEIGHT;
  pLayerCfg.PixelFormat = BSP_DISPLAY_PIXEL_FORMAT;
  pLayerCfg.Alpha = 255;
  pLayerCfg.Alpha0 = 0;
  pLayerCfg.BlendingFactor1 = LTDC_BLENDING_FACTOR1_PAxCA;
  pLayerCfg.BlendingFactor2 = LTDC_BLENDING_FACTOR2_PAxCA;
  pLayerCfg.FBStartAdress = BSP_DISPLAY_FRAMEBUFFER_ADDR;
  pLayerCfg.ImageWidth = BSP_DISPLAY_WIDTH;
  pLayerCfg.ImageHeight = BSP_DISPLAY_HEIGHT;
  pLayerCfg.Backcolor.Blue = 0;
  pLayerCfg.Backcolor.Green = 0;
  pLayerCfg.Backcolor.Red = 0;
  if (HAL_LTDC_ConfigLayer(&hltdc, &pLayerCfg, 0) != HAL_OK)
  {
    Error_Handler();
  }

  bsp_ltdc_initialized = 1U;
}

uint32_t *BSP_Display_GetFrameBuffer(void)
{
  return (uint32_t *)BSP_DISPLAY_FRAMEBUFFER_ADDR;
}

void BSP_Display_FillColor(uint32_t color)
{
  uint32_t *fb = BSP_Display_GetFrameBuffer();
  const uint32_t pixel_count = BSP_DISPLAY_WIDTH * BSP_DISPLAY_HEIGHT;

  for (uint32_t i = 0; i < pixel_count; i++)
  {
    fb[i] = color;
  }
}

void BSP_Display_FillTestPattern(void)
{
  uint32_t *fb = BSP_Display_GetFrameBuffer();
  static const uint32_t colors[] = {
    0xFFFF0000U,
    0xFF00FF00U,
    0xFF0000FFU,
    0xFFFFFF00U,
    0xFFFF00FFU,
    0xFF00FFFFU,
    0xFFFFFFFFU,
    0xFF000000U
  };

  for (uint32_t y = 0; y < BSP_DISPLAY_HEIGHT; y++)
  {
    for (uint32_t x = 0; x < BSP_DISPLAY_WIDTH; x++)
    {
      const uint32_t bar = BSP_Display_GetColorBarIndex(x);
      fb[(y * BSP_DISPLAY_WIDTH) + x] = colors[bar];
    }
  }
}

void BSP_Display_MspInit(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

  if (bsp_ltdc_msp_initialized != 0U)
  {
    return;
  }

  __HAL_RCC_GPIOI_CLK_ENABLE();
  __HAL_RCC_GPIOJ_CLK_ENABLE();
  __HAL_RCC_GPIOK_CLK_ENABLE();

  PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_LTDC;
  PeriphClkInitStruct.PLL3.PLL3M = 25;
  PeriphClkInitStruct.PLL3.PLL3N = 270;
  PeriphClkInitStruct.PLL3.PLL3P = 2;
  PeriphClkInitStruct.PLL3.PLL3Q = 2;
  PeriphClkInitStruct.PLL3.PLL3R = 10;
  PeriphClkInitStruct.PLL3.PLL3RGE = RCC_PLL3VCIRANGE_0;
  PeriphClkInitStruct.PLL3.PLL3VCOSEL = RCC_PLL3VCOWIDE;
  PeriphClkInitStruct.PLL3.PLL3FRACN = 0.0;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  __HAL_RCC_LTDC_CLK_ENABLE();

  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF14_LTDC;

  GPIO_InitStruct.Pin = GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
  HAL_GPIO_Init(GPIOI, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3
                      | GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7
                      | GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11
                      | GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
  HAL_GPIO_Init(GPIOJ, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3
                      | GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7;
  HAL_GPIO_Init(GPIOK, &GPIO_InitStruct);

  HAL_NVIC_SetPriority(LTDC_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(LTDC_IRQn);

  bsp_ltdc_msp_initialized = 1U;
}

void BSP_Display_MspDeInit(void)
{
  if (bsp_ltdc_msp_initialized == 0U)
  {
    return;
  }

  __HAL_RCC_LTDC_CLK_DISABLE();
  HAL_GPIO_DeInit(GPIOI, GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15);
  HAL_GPIO_DeInit(GPIOJ, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3
                        | GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7
                        | GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11
                        | GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15);
  HAL_GPIO_DeInit(GPIOK, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3
                        | GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7);
  HAL_NVIC_DisableIRQ(LTDC_IRQn);

  bsp_ltdc_msp_initialized = 0U;
}

void HAL_LTDC_MspInit(LTDC_HandleTypeDef *ltdcHandle)
{
  if (ltdcHandle->Instance == LTDC)
  {
    BSP_Display_MspInit();
  }
}

void HAL_LTDC_MspDeInit(LTDC_HandleTypeDef *ltdcHandle)
{
  if (ltdcHandle->Instance == LTDC)
  {
    BSP_Display_MspDeInit();
  }
}

static uint32_t BSP_Display_GetColorBarIndex(uint32_t x)
{
  const uint32_t bar_count = 8U;
  const uint32_t bar_width = BSP_DISPLAY_WIDTH / bar_count;
  uint32_t bar = x / bar_width;

  if (bar >= bar_count)
  {
    bar = bar_count - 1U;
  }

  return bar;
}
