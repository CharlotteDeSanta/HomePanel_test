#include "bsp_board.h"

#include "bsp_sdram.h"
#include "gpio.h"

#define BSP_LED_R_PIN GPIO_PIN_0
#define BSP_LED_R_PORT GPIOB
#define BSP_LED_G_PIN GPIO_PIN_1
#define BSP_LED_G_PORT GPIOB
#define BSP_LED_B_PIN GPIO_PIN_3
#define BSP_LED_B_PORT GPIOA
#define BSP_LCD_BL_PIN GPIO_PIN_13
#define BSP_LCD_BL_PORT GPIOD
#define BSP_LCD_RST_PIN GPIO_PIN_7
#define BSP_LCD_RST_PORT GPIOG

static uint32_t bsp_board_initialized = 0U;

void MX_GPIO_Init(void)
{
  BSP_Board_Init();
}

void BSP_Board_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  if (bsp_board_initialized != 0U)
  {
    return;
  }

  BSP_Board_MPU_Config();

  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOG_CLK_ENABLE();

  GPIO_InitStruct.Pin = BSP_LED_R_PIN | BSP_LED_G_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = BSP_LED_B_PIN;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = BSP_LCD_BL_PIN;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = BSP_LCD_RST_PIN;
  HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

  BSP_Board_SetStatusLed(0U, 0U, 0U);
  HAL_GPIO_WritePin(BSP_LCD_BL_PORT, BSP_LCD_BL_PIN, GPIO_PIN_SET);
  HAL_GPIO_WritePin(BSP_LCD_RST_PORT, BSP_LCD_RST_PIN, GPIO_PIN_SET);
  BSP_Board_ResetDisplay();

  bsp_board_initialized = 1U;
}

void BSP_Board_SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);

  while (!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY))
  {
  }

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 5;
  RCC_OscInitStruct.PLL.PLLN = 192;
  RCC_OscInitStruct.PLL.PLLP = 2;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_2;
  RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
  RCC_OscInitStruct.PLL.PLLFRACN = 0;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                              | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2
                              | RCC_CLOCKTYPE_D3PCLK1 | RCC_CLOCKTYPE_D1PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

void BSP_Board_ResetDisplay(void)
{
  HAL_GPIO_WritePin(BSP_LCD_RST_PORT, BSP_LCD_RST_PIN, GPIO_PIN_RESET);
  HAL_Delay(20);
  HAL_GPIO_WritePin(BSP_LCD_RST_PORT, BSP_LCD_RST_PIN, GPIO_PIN_SET);
  HAL_Delay(120);
}

void BSP_Board_SetStatusLed(uint32_t red_on, uint32_t green_on, uint32_t blue_on)
{
  HAL_GPIO_WritePin(BSP_LED_R_PORT, BSP_LED_R_PIN,
                    red_on ? GPIO_PIN_RESET : GPIO_PIN_SET);
  HAL_GPIO_WritePin(BSP_LED_G_PORT, BSP_LED_G_PIN,
                    green_on ? GPIO_PIN_RESET : GPIO_PIN_SET);
  HAL_GPIO_WritePin(BSP_LED_B_PORT, BSP_LED_B_PIN,
                    blue_on ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

void BSP_Board_MPU_Config(void)
{
  MPU_Region_InitTypeDef MPU_InitStruct = {0};

  HAL_MPU_Disable();

  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER0;
  MPU_InitStruct.BaseAddress = 0x0;
  MPU_InitStruct.Size = MPU_REGION_SIZE_4GB;
  MPU_InitStruct.SubRegionDisable = 0x87;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
  MPU_InitStruct.AccessPermission = MPU_REGION_NO_ACCESS;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_SHAREABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;
  HAL_MPU_ConfigRegion(&MPU_InitStruct);

  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER1;
  MPU_InitStruct.BaseAddress = BSP_SDRAM_BASE_ADDR;
  MPU_InitStruct.Size = MPU_REGION_SIZE_64MB;
  MPU_InitStruct.SubRegionDisable = 0x00;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
  MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_NOT_SHAREABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_CACHEABLE;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;
  HAL_MPU_ConfigRegion(&MPU_InitStruct);

  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);
}

void BSP_Board_FatalError(void)
{
  BSP_Board_SetStatusLed(1U, 0U, 0U);
  __disable_irq();
  while (1)
  {
  }
}
