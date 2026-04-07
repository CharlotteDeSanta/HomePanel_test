#include "bsp_qspi_flash.h"

#include "main.h"
#include "stm32h7xx_hal_qspi.h"

#define BSP_QSPI_CMD_RESET_ENABLE            0x66U
#define BSP_QSPI_CMD_RESET_MEMORY            0x99U
#define BSP_QSPI_CMD_WRITE_ENABLE            0x06U
#define BSP_QSPI_CMD_READ_STATUS_REG1        0x05U
#define BSP_QSPI_CMD_WRITE_STATUS_REG1       0x01U
#define BSP_QSPI_CMD_ENTER_4BYTE_ADDR_MODE   0xB7U
#define BSP_QSPI_CMD_READ_4BYTE              0x03U
#define BSP_QSPI_CMD_QUAD_IO_READ_4BYTE      0xECU

#define BSP_QSPI_SR1_BUSY                    0x01U
#define BSP_QSPI_SR1_WEL                     0x02U

#define BSP_QSPI_TIMEOUT_MS                  5000U

static QSPI_HandleTypeDef hqspi;
static uint32_t bsp_qspi_initialized = 0U;
static uint32_t bsp_qspi_memory_mapped = 0U;
static uint32_t bsp_qspi_msp_initialized = 0U;

static HAL_StatusTypeDef BSP_QSPI_Configure(uint32_t flash_id, uint32_t dual_flash, uint32_t enable_quad_mode);
static HAL_StatusTypeDef BSP_QSPI_WriteEnable(void);
static HAL_StatusTypeDef BSP_QSPI_WaitWhileBusy(uint32_t timeout_ms);
static HAL_StatusTypeDef BSP_QSPI_ResetMemory(void);
static HAL_StatusTypeDef BSP_QSPI_EnableQuadMode(void);
static HAL_StatusTypeDef BSP_QSPI_EnterFourByteAddressMode(void);

HAL_StatusTypeDef BSP_QSPI_Init(void)
{
  if (bsp_qspi_initialized != 0U)
  {
    return HAL_OK;
  }

  return BSP_QSPI_Configure(QSPI_FLASH_ID_1, QSPI_DUALFLASH_DISABLE, 1U);
}

static HAL_StatusTypeDef BSP_QSPI_Configure(uint32_t flash_id, uint32_t dual_flash, uint32_t enable_quad_mode)
{
  const uint32_t is_dual_flash = (dual_flash != QSPI_DUALFLASH_DISABLE) ? 1U : 0U;

  hqspi.Instance = QUADSPI;
  /*
   * Keep the bus slow while we stabilize the external flash path.
   * PLL2_R is 240 MHz in the current clock tree. Prescaler 7 gives 30 MHz.
   */
  hqspi.Init.ClockPrescaler = 7;
  /*
   * The single-flash reference example uses a deeper FIFO threshold, 6-cycle CS
   * high time, clock mode 3, and FSIZE=24 for a 32 MiB W25Q256. Keeping the
   * old dual-flash FSIZE=25 prevents memory-mapped mode from enabling cleanly.
   */
  hqspi.Init.FifoThreshold = is_dual_flash ? 1U : 24U;
  hqspi.Init.SampleShifting = QSPI_SAMPLE_SHIFTING_HALFCYCLE;
  hqspi.Init.FlashSize = is_dual_flash ? 25U : 24U;
  hqspi.Init.ChipSelectHighTime = is_dual_flash ? QSPI_CS_HIGH_TIME_5_CYCLE : QSPI_CS_HIGH_TIME_6_CYCLE;
  hqspi.Init.ClockMode = is_dual_flash ? QSPI_CLOCK_MODE_0 : QSPI_CLOCK_MODE_3;
  hqspi.Init.FlashID = flash_id;
  hqspi.Init.DualFlash = dual_flash;

  if (HAL_QSPI_Init(&hqspi) != HAL_OK)
  {
    return HAL_ERROR;
  }

  if ((enable_quad_mode != 0U) && (BSP_QSPI_EnableQuadMode() != HAL_OK))
  {
    return HAL_ERROR;
  }

  if (BSP_QSPI_ResetMemory() != HAL_OK)
  {
    return HAL_ERROR;
  }

  if (BSP_QSPI_EnterFourByteAddressMode() != HAL_OK)
  {
    return HAL_ERROR;
  }

  bsp_qspi_initialized = 1U;
  return HAL_OK;
}

HAL_StatusTypeDef BSP_QSPI_EnableMemoryMappedMode(void)
{
  QSPI_CommandTypeDef command = {0};
  QSPI_MemoryMappedTypeDef memory_mapped_cfg = {0};

  if (bsp_qspi_memory_mapped != 0U)
  {
    return HAL_OK;
  }

  if (HAL_QSPI_Abort(&hqspi) != HAL_OK)
  {
    /* The peripheral may already be idle. Keep going and let HAL report a
     * real error if memory-mapped mode still cannot be entered.
     */
  }

  __HAL_QSPI_CLEAR_FLAG(&hqspi, QSPI_FLAG_TC | QSPI_FLAG_TE | QSPI_FLAG_SM | QSPI_FLAG_TO | QSPI_FLAG_FT);

  command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
  /*
   * Command-mode reads are already verified to be correct in single-flash mode.
   * Switch memory-mapped access back to the vendor example's fast 4-4-4 read
   * sequence, because HAL_QSPI_MemoryMapped() does not come up reliably here
   * with the slow 1-1-1 configuration.
   */
  command.Instruction = BSP_QSPI_CMD_QUAD_IO_READ_4BYTE;
  command.AddressMode = QSPI_ADDRESS_4_LINES;
  command.AddressSize = QSPI_ADDRESS_32_BITS;
  command.Address = 0U;
  command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
  command.DataMode = QSPI_DATA_4_LINES;
  command.DummyCycles = 6;
  command.DdrMode = QSPI_DDR_MODE_DISABLE;
  command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
  command.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;

  memory_mapped_cfg.TimeOutActivation = QSPI_TIMEOUT_COUNTER_DISABLE;
  memory_mapped_cfg.TimeOutPeriod = 0;

  if (HAL_QSPI_MemoryMapped(&hqspi, &command, &memory_mapped_cfg) != HAL_OK)
  {
    return HAL_ERROR;
  }

  bsp_qspi_memory_mapped = 1U;
  return HAL_OK;
}

HAL_StatusTypeDef BSP_QSPI_Read(uint8_t *data, uint32_t address, uint32_t size)
{
  QSPI_CommandTypeDef command = {0};

  if ((data == NULL) || (size == 0U))
  {
    return HAL_OK;
  }

  command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
  command.Instruction = BSP_QSPI_CMD_READ_4BYTE;
  command.AddressMode = QSPI_ADDRESS_1_LINE;
  command.AddressSize = QSPI_ADDRESS_32_BITS;
  command.Address = address;
  command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
  command.DataMode = QSPI_DATA_1_LINE;
  command.DummyCycles = 0;
  command.NbData = size;
  command.DdrMode = QSPI_DDR_MODE_DISABLE;
  command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
  command.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;

  if (HAL_QSPI_Command(&hqspi, &command, BSP_QSPI_TIMEOUT_MS) != HAL_OK)
  {
    return HAL_ERROR;
  }

  if (HAL_QSPI_Receive(&hqspi, data, BSP_QSPI_TIMEOUT_MS) != HAL_OK)
  {
    return HAL_ERROR;
  }

  return HAL_OK;
}
void BSP_QSPI_MspInit(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

  if (bsp_qspi_msp_initialized != 0U)
  {
    return;
  }

  PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_QSPI;
  /*
   * Keep QSPI on the same PLL2R configuration that FMC/SDRAM already uses.
   * Re-selecting PLL2 without providing dividers can destabilize memory-mapped
   * reads even if basic access still appears to work.
   */
  PeriphClkInitStruct.PLL2.PLL2M = 5;
  PeriphClkInitStruct.PLL2.PLL2N = 144;
  PeriphClkInitStruct.PLL2.PLL2P = 2;
  PeriphClkInitStruct.PLL2.PLL2Q = 2;
  PeriphClkInitStruct.PLL2.PLL2R = 3;
  PeriphClkInitStruct.PLL2.PLL2RGE = RCC_PLL2VCIRANGE_2;
  PeriphClkInitStruct.PLL2.PLL2VCOSEL = RCC_PLL2VCOWIDE;
  PeriphClkInitStruct.PLL2.PLL2FRACN = 0;
  PeriphClkInitStruct.QspiClockSelection = RCC_QSPICLKSOURCE_PLL2;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  __HAL_RCC_QSPI_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOG_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();

  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;

  GPIO_InitStruct.Pin = GPIO_PIN_2;
  GPIO_InitStruct.Alternate = GPIO_AF9_QUADSPI;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = GPIO_PIN_6 | GPIO_PIN_7;
  GPIO_InitStruct.Alternate = GPIO_AF9_QUADSPI;
  HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = GPIO_PIN_8 | GPIO_PIN_9;
  GPIO_InitStruct.Alternate = GPIO_AF10_QUADSPI;
  HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = GPIO_PIN_6;
  GPIO_InitStruct.Alternate = GPIO_AF10_QUADSPI;
  HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = GPIO_PIN_9 | GPIO_PIN_14;
  GPIO_InitStruct.Alternate = GPIO_AF9_QUADSPI;
  HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = GPIO_PIN_2 | GPIO_PIN_3;
  GPIO_InitStruct.Alternate = GPIO_AF9_QUADSPI;
  HAL_GPIO_Init(GPIOH, &GPIO_InitStruct);

  bsp_qspi_msp_initialized = 1U;
}

void BSP_QSPI_MspDeInit(void)
{
  if (bsp_qspi_msp_initialized == 0U)
  {
    return;
  }

  __HAL_RCC_QSPI_CLK_DISABLE();

  HAL_GPIO_DeInit(GPIOB, GPIO_PIN_2);
  HAL_GPIO_DeInit(GPIOF, GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9);
  HAL_GPIO_DeInit(GPIOG, GPIO_PIN_6 | GPIO_PIN_9 | GPIO_PIN_14);
  HAL_GPIO_DeInit(GPIOH, GPIO_PIN_2 | GPIO_PIN_3);

  bsp_qspi_msp_initialized = 0U;
}

void HAL_QSPI_MspInit(QSPI_HandleTypeDef *qspiHandle)
{
  if (qspiHandle->Instance == QUADSPI)
  {
    BSP_QSPI_MspInit();
  }
}

void HAL_QSPI_MspDeInit(QSPI_HandleTypeDef *qspiHandle)
{
  if (qspiHandle->Instance == QUADSPI)
  {
    BSP_QSPI_MspDeInit();
  }
}

static HAL_StatusTypeDef BSP_QSPI_WriteEnable(void)
{
  QSPI_CommandTypeDef command = {0};
  QSPI_AutoPollingTypeDef config = {0};

  command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
  command.Instruction = BSP_QSPI_CMD_WRITE_ENABLE;
  command.AddressMode = QSPI_ADDRESS_NONE;
  command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
  command.DataMode = QSPI_DATA_NONE;
  command.DummyCycles = 0;
  command.DdrMode = QSPI_DDR_MODE_DISABLE;
  command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
  command.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;

  if (HAL_QSPI_Command(&hqspi, &command, BSP_QSPI_TIMEOUT_MS) != HAL_OK)
  {
    return HAL_ERROR;
  }

  config.Match = BSP_QSPI_SR1_WEL;
  config.Mask = BSP_QSPI_SR1_WEL;
  config.MatchMode = QSPI_MATCH_MODE_AND;
  config.StatusBytesSize = 1;
  config.Interval = 0x10;
  config.AutomaticStop = QSPI_AUTOMATIC_STOP_ENABLE;

  command.Instruction = BSP_QSPI_CMD_READ_STATUS_REG1;
  command.DataMode = QSPI_DATA_1_LINE;
  command.NbData = 1;

  if (HAL_QSPI_AutoPolling(&hqspi, &command, &config, BSP_QSPI_TIMEOUT_MS) != HAL_OK)
  {
    return HAL_ERROR;
  }

  return HAL_OK;
}

static HAL_StatusTypeDef BSP_QSPI_WaitWhileBusy(uint32_t timeout_ms)
{
  QSPI_CommandTypeDef command = {0};
  QSPI_AutoPollingTypeDef config = {0};

  command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
  command.Instruction = BSP_QSPI_CMD_READ_STATUS_REG1;
  command.AddressMode = QSPI_ADDRESS_NONE;
  command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
  command.DataMode = QSPI_DATA_1_LINE;
  command.DummyCycles = 0;
  command.DdrMode = QSPI_DDR_MODE_DISABLE;
  command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
  command.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;

  config.Match = 0x00;
  config.Mask = BSP_QSPI_SR1_BUSY;
  config.MatchMode = QSPI_MATCH_MODE_AND;
  config.StatusBytesSize = 1;
  config.Interval = 0x10;
  config.AutomaticStop = QSPI_AUTOMATIC_STOP_ENABLE;

  if (HAL_QSPI_AutoPolling(&hqspi, &command, &config, timeout_ms) != HAL_OK)
  {
    return HAL_ERROR;
  }

  return HAL_OK;
}

static HAL_StatusTypeDef BSP_QSPI_ResetMemory(void)
{
  QSPI_CommandTypeDef command = {0};

  command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
  command.Instruction = BSP_QSPI_CMD_RESET_ENABLE;
  command.AddressMode = QSPI_ADDRESS_NONE;
  command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
  command.DataMode = QSPI_DATA_NONE;
  command.DummyCycles = 0;
  command.DdrMode = QSPI_DDR_MODE_DISABLE;
  command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
  command.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;

  if (HAL_QSPI_Command(&hqspi, &command, BSP_QSPI_TIMEOUT_MS) != HAL_OK)
  {
    return HAL_ERROR;
  }

  command.Instruction = BSP_QSPI_CMD_RESET_MEMORY;
  if (HAL_QSPI_Command(&hqspi, &command, BSP_QSPI_TIMEOUT_MS) != HAL_OK)
  {
    return HAL_ERROR;
  }

  return BSP_QSPI_WaitWhileBusy(BSP_QSPI_TIMEOUT_MS);
}

static HAL_StatusTypeDef BSP_QSPI_EnableQuadMode(void)
{
  QSPI_CommandTypeDef command = {0};
  uint8_t status_value = 0x06U;

  if (BSP_QSPI_WriteEnable() != HAL_OK)
  {
    return HAL_ERROR;
  }

  command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
  command.Instruction = BSP_QSPI_CMD_WRITE_STATUS_REG1;
  command.AddressMode = QSPI_ADDRESS_NONE;
  command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
  command.DataMode = QSPI_DATA_1_LINE;
  command.DummyCycles = 0;
  command.NbData = 1;
  command.DdrMode = QSPI_DDR_MODE_DISABLE;
  command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
  command.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;

  if (HAL_QSPI_Command(&hqspi, &command, BSP_QSPI_TIMEOUT_MS) != HAL_OK)
  {
    return HAL_ERROR;
  }

  if (HAL_QSPI_Transmit(&hqspi, &status_value, BSP_QSPI_TIMEOUT_MS) != HAL_OK)
  {
    return HAL_ERROR;
  }

  return BSP_QSPI_WaitWhileBusy(BSP_QSPI_TIMEOUT_MS);
}

static HAL_StatusTypeDef BSP_QSPI_EnterFourByteAddressMode(void)
{
  QSPI_CommandTypeDef command = {0};

  if (BSP_QSPI_WriteEnable() != HAL_OK)
  {
    return HAL_ERROR;
  }

  command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
  command.Instruction = BSP_QSPI_CMD_ENTER_4BYTE_ADDR_MODE;
  command.AddressMode = QSPI_ADDRESS_NONE;
  command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
  command.DataMode = QSPI_DATA_NONE;
  command.DummyCycles = 0;
  command.DdrMode = QSPI_DDR_MODE_DISABLE;
  command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
  command.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;

  if (HAL_QSPI_Command(&hqspi, &command, BSP_QSPI_TIMEOUT_MS) != HAL_OK)
  {
    return HAL_ERROR;
  }

  return BSP_QSPI_WaitWhileBusy(BSP_QSPI_TIMEOUT_MS);
}
