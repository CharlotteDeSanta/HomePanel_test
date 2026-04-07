#include "bsp_touch_gt911.h"

#include <string.h>

#include "bsp_config.h"

#define BSP_TOUCH_I2C_ADDRESS_0    0xBAU
#define BSP_TOUCH_I2C_ADDRESS_1    0x28U
#define BSP_TOUCH_REG_PRODUCT_ID   0x8140U
#define BSP_TOUCH_REG_CONFIG       0x8047U
#define BSP_TOUCH_REG_COORD        0x814EU
#define BSP_TOUCH_MAX_POINTS       5U

#define BSP_TOUCH_SCL_PIN          GPIO_PIN_4
#define BSP_TOUCH_SCL_PORT         GPIOH
#define BSP_TOUCH_SCL_CLK_ENABLE() __HAL_RCC_GPIOH_CLK_ENABLE()

#define BSP_TOUCH_SDA_PIN          GPIO_PIN_5
#define BSP_TOUCH_SDA_PORT         GPIOH
#define BSP_TOUCH_SDA_CLK_ENABLE() __HAL_RCC_GPIOH_CLK_ENABLE()

#define BSP_TOUCH_RST_PIN          GPIO_PIN_7
#define BSP_TOUCH_RST_PORT         GPIOG
#define BSP_TOUCH_RST_CLK_ENABLE() __HAL_RCC_GPIOG_CLK_ENABLE()

#define BSP_TOUCH_INT_PIN          GPIO_PIN_3
#define BSP_TOUCH_INT_PORT         GPIOG
#define BSP_TOUCH_INT_CLK_ENABLE() __HAL_RCC_GPIOG_CLK_ENABLE()

#define BSP_TOUCH_SWAP_XY          0U
#define BSP_TOUCH_INVERT_X         0U
#define BSP_TOUCH_INVERT_Y         0U
#define BSP_TOUCH_IDLE_POLL_MS     32U
#define BSP_TOUCH_ACTIVE_POLL_MS   8U

static uint32_t bsp_touch_initialized = 0U;
static uint32_t bsp_touch_ready = 0U;
static uint8_t bsp_touch_i2c_address = BSP_TOUCH_I2C_ADDRESS_0;
static uint32_t bsp_touch_last_poll_tick = 0U;
static uint8_t bsp_touch_last_touch_active = 0U;
static int32_t bsp_touch_cached_x = 0;
static int32_t bsp_touch_cached_y = 0;

static const uint8_t bsp_touch_gt911_config[] = {
  0x41,0x20,0x03,0xE0,0x01,0x05,0x3D,0x00,0x01,0x08,
  0x1E,0x05,0x3C,0x3C,0x03,0x05,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x1A,0x1C,0x1E,0x14,0x8A,0x2A,0x0C,
  0x2A,0x28,0xEB,0x04,0x00,0x00,0x01,0x61,0x03,0x2C,
  0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x14,0x3C,0x94,0xC5,0x02,0x08,0x00,0x00,0x04,
  0xB7,0x16,0x00,0x9F,0x1B,0x00,0x8B,0x22,0x00,0x7B,
  0x2B,0x00,0x70,0x36,0x00,0x70,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x18,0x16,0x14,0x12,0x10,0x0E,0x0C,0x0A,
  0x08,0x06,0x04,0x02,0xFF,0xFF,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x24,0x22,0x21,0x20,0x1F,0x1E,0x1D,0x1C,
  0x18,0x16,0x13,0x12,0x10,0x0F,0x0A,0x08,0x06,0x04,
  0x02,0x00,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00
};

BSP_Touch_DebugStatus g_bsp_touch_debug_status = {0};

static void BSP_Touch_DelayShort(void);
static void BSP_Touch_I2C_Start(void);
static void BSP_Touch_I2C_Stop(void);
static void BSP_Touch_I2C_SendByte(uint8_t value);
static uint8_t BSP_Touch_I2C_ReadByte(void);
static uint8_t BSP_Touch_I2C_WaitAck(void);
static void BSP_Touch_I2C_Ack(void);
static void BSP_Touch_I2C_NAck(void);
static uint8_t BSP_Touch_I2C_WriteBytes(uint8_t client_addr, const uint8_t *buffer, uint16_t length);
static uint8_t BSP_Touch_I2C_ReadBytes(uint8_t client_addr, uint8_t *buffer, uint16_t length);
static HAL_StatusTypeDef BSP_Touch_WriteRegister(uint8_t client_addr, uint16_t reg, const uint8_t *data, uint16_t length);
static HAL_StatusTypeDef BSP_Touch_ReadRegister(uint8_t client_addr, uint16_t reg, uint8_t *data, uint16_t length);
static void BSP_Touch_ConfigurePins(void);
static void BSP_Touch_ResetController(GPIO_PinState int_level);
static HAL_StatusTypeDef BSP_Touch_Probe(uint8_t *product_id);
static void BSP_Touch_TransformCoordinates(int32_t *x, int32_t *y);
static HAL_StatusTypeDef BSP_Touch_WriteDefaultConfig(void);

static void BSP_Touch_DelayShort(void)
{
  for (volatile uint32_t i = 0; i < 160U; ++i)
  {
    __NOP();
  }
}

static void BSP_Touch_I2C_Start(void)
{
  HAL_GPIO_WritePin(BSP_TOUCH_SDA_PORT, BSP_TOUCH_SDA_PIN, GPIO_PIN_SET);
  HAL_GPIO_WritePin(BSP_TOUCH_SCL_PORT, BSP_TOUCH_SCL_PIN, GPIO_PIN_SET);
  BSP_Touch_DelayShort();
  HAL_GPIO_WritePin(BSP_TOUCH_SDA_PORT, BSP_TOUCH_SDA_PIN, GPIO_PIN_RESET);
  BSP_Touch_DelayShort();
  HAL_GPIO_WritePin(BSP_TOUCH_SCL_PORT, BSP_TOUCH_SCL_PIN, GPIO_PIN_RESET);
  BSP_Touch_DelayShort();
}

static void BSP_Touch_I2C_Stop(void)
{
  HAL_GPIO_WritePin(BSP_TOUCH_SDA_PORT, BSP_TOUCH_SDA_PIN, GPIO_PIN_RESET);
  BSP_Touch_DelayShort();
  HAL_GPIO_WritePin(BSP_TOUCH_SCL_PORT, BSP_TOUCH_SCL_PIN, GPIO_PIN_SET);
  BSP_Touch_DelayShort();
  HAL_GPIO_WritePin(BSP_TOUCH_SDA_PORT, BSP_TOUCH_SDA_PIN, GPIO_PIN_SET);
  BSP_Touch_DelayShort();
}

static void BSP_Touch_I2C_SendByte(uint8_t value)
{
  for (uint32_t i = 0U; i < 8U; ++i)
  {
    HAL_GPIO_WritePin(BSP_TOUCH_SDA_PORT, BSP_TOUCH_SDA_PIN,
                      (value & 0x80U) != 0U ? GPIO_PIN_SET : GPIO_PIN_RESET);
    BSP_Touch_DelayShort();
    HAL_GPIO_WritePin(BSP_TOUCH_SCL_PORT, BSP_TOUCH_SCL_PIN, GPIO_PIN_SET);
    BSP_Touch_DelayShort();
    HAL_GPIO_WritePin(BSP_TOUCH_SCL_PORT, BSP_TOUCH_SCL_PIN, GPIO_PIN_RESET);
    value <<= 1;
    BSP_Touch_DelayShort();
  }

  HAL_GPIO_WritePin(BSP_TOUCH_SDA_PORT, BSP_TOUCH_SDA_PIN, GPIO_PIN_SET);
}

static uint8_t BSP_Touch_I2C_ReadByte(void)
{
  uint8_t value = 0U;

  HAL_GPIO_WritePin(BSP_TOUCH_SDA_PORT, BSP_TOUCH_SDA_PIN, GPIO_PIN_SET);

  for (uint32_t i = 0U; i < 8U; ++i)
  {
    value <<= 1;
    HAL_GPIO_WritePin(BSP_TOUCH_SCL_PORT, BSP_TOUCH_SCL_PIN, GPIO_PIN_SET);
    BSP_Touch_DelayShort();
    if (HAL_GPIO_ReadPin(BSP_TOUCH_SDA_PORT, BSP_TOUCH_SDA_PIN) == GPIO_PIN_SET)
    {
      value |= 0x01U;
    }
    HAL_GPIO_WritePin(BSP_TOUCH_SCL_PORT, BSP_TOUCH_SCL_PIN, GPIO_PIN_RESET);
    BSP_Touch_DelayShort();
  }

  return value;
}

static uint8_t BSP_Touch_I2C_WaitAck(void)
{
  uint8_t ack = 1U;

  HAL_GPIO_WritePin(BSP_TOUCH_SDA_PORT, BSP_TOUCH_SDA_PIN, GPIO_PIN_SET);
  BSP_Touch_DelayShort();
  HAL_GPIO_WritePin(BSP_TOUCH_SCL_PORT, BSP_TOUCH_SCL_PIN, GPIO_PIN_SET);
  BSP_Touch_DelayShort();
  if (HAL_GPIO_ReadPin(BSP_TOUCH_SDA_PORT, BSP_TOUCH_SDA_PIN) == GPIO_PIN_RESET)
  {
    ack = 0U;
  }
  HAL_GPIO_WritePin(BSP_TOUCH_SCL_PORT, BSP_TOUCH_SCL_PIN, GPIO_PIN_RESET);
  BSP_Touch_DelayShort();

  return ack;
}

static void BSP_Touch_I2C_Ack(void)
{
  HAL_GPIO_WritePin(BSP_TOUCH_SDA_PORT, BSP_TOUCH_SDA_PIN, GPIO_PIN_RESET);
  BSP_Touch_DelayShort();
  HAL_GPIO_WritePin(BSP_TOUCH_SCL_PORT, BSP_TOUCH_SCL_PIN, GPIO_PIN_SET);
  BSP_Touch_DelayShort();
  HAL_GPIO_WritePin(BSP_TOUCH_SCL_PORT, BSP_TOUCH_SCL_PIN, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(BSP_TOUCH_SDA_PORT, BSP_TOUCH_SDA_PIN, GPIO_PIN_SET);
  BSP_Touch_DelayShort();
}

static void BSP_Touch_I2C_NAck(void)
{
  HAL_GPIO_WritePin(BSP_TOUCH_SDA_PORT, BSP_TOUCH_SDA_PIN, GPIO_PIN_SET);
  BSP_Touch_DelayShort();
  HAL_GPIO_WritePin(BSP_TOUCH_SCL_PORT, BSP_TOUCH_SCL_PIN, GPIO_PIN_SET);
  BSP_Touch_DelayShort();
  HAL_GPIO_WritePin(BSP_TOUCH_SCL_PORT, BSP_TOUCH_SCL_PIN, GPIO_PIN_RESET);
  BSP_Touch_DelayShort();
}

static uint8_t BSP_Touch_I2C_WriteBytes(uint8_t client_addr, const uint8_t *buffer, uint16_t length)
{
  BSP_Touch_I2C_Stop();

  for (uint32_t attempt = 0U; attempt < 1000U; ++attempt)
  {
    BSP_Touch_I2C_Start();
    BSP_Touch_I2C_SendByte(client_addr & 0xFEU);
    if (BSP_Touch_I2C_WaitAck() == 0U)
    {
      for (uint16_t i = 0U; i < length; ++i)
      {
        BSP_Touch_I2C_SendByte(buffer[i]);
        if (BSP_Touch_I2C_WaitAck() != 0U)
        {
          BSP_Touch_I2C_Stop();
          return 1U;
        }
      }
      BSP_Touch_I2C_Stop();
      return 0U;
    }
    BSP_Touch_I2C_Stop();
  }

  return 1U;
}

static uint8_t BSP_Touch_I2C_ReadBytes(uint8_t client_addr, uint8_t *buffer, uint16_t length)
{
  BSP_Touch_I2C_Start();
  BSP_Touch_I2C_SendByte(client_addr | 0x01U);
  if (BSP_Touch_I2C_WaitAck() != 0U)
  {
    BSP_Touch_I2C_Stop();
    return 1U;
  }

  for (uint16_t i = 0U; i < length; ++i)
  {
    buffer[i] = BSP_Touch_I2C_ReadByte();
    if (i + 1U == length)
    {
      BSP_Touch_I2C_NAck();
    }
    else
    {
      BSP_Touch_I2C_Ack();
    }
  }

  BSP_Touch_I2C_Stop();
  return 0U;
}

static HAL_StatusTypeDef BSP_Touch_WriteRegister(uint8_t client_addr, uint16_t reg, const uint8_t *data, uint16_t length)
{
  uint8_t buffer[260];

  if (length > (sizeof(buffer) - 2U))
  {
    return HAL_ERROR;
  }

  buffer[0] = (uint8_t)(reg >> 8);
  buffer[1] = (uint8_t)(reg & 0xFFU);
  for (uint16_t i = 0U; i < length; ++i)
  {
    buffer[2U + i] = data[i];
  }

  return BSP_Touch_I2C_WriteBytes(client_addr, buffer, (uint16_t)(length + 2U)) == 0U
           ? HAL_OK
           : HAL_ERROR;
}

static HAL_StatusTypeDef BSP_Touch_ReadRegister(uint8_t client_addr, uint16_t reg, uint8_t *data, uint16_t length)
{
  uint8_t addr[2];

  addr[0] = (uint8_t)(reg >> 8);
  addr[1] = (uint8_t)(reg & 0xFFU);

  if (BSP_Touch_I2C_WriteBytes(client_addr, addr, sizeof(addr)) != 0U)
  {
    return HAL_ERROR;
  }

  return BSP_Touch_I2C_ReadBytes(client_addr, data, length) == 0U ? HAL_OK : HAL_ERROR;
}

static void BSP_Touch_ConfigurePins(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  BSP_TOUCH_SCL_CLK_ENABLE();
  BSP_TOUCH_SDA_CLK_ENABLE();
  BSP_TOUCH_RST_CLK_ENABLE();
  BSP_TOUCH_INT_CLK_ENABLE();

  GPIO_InitStruct.Pin = BSP_TOUCH_SCL_PIN | BSP_TOUCH_SDA_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(GPIOH, &GPIO_InitStruct);

  HAL_GPIO_WritePin(BSP_TOUCH_SCL_PORT, BSP_TOUCH_SCL_PIN, GPIO_PIN_SET);
  HAL_GPIO_WritePin(BSP_TOUCH_SDA_PORT, BSP_TOUCH_SDA_PIN, GPIO_PIN_SET);

  GPIO_InitStruct.Pin = BSP_TOUCH_RST_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(BSP_TOUCH_RST_PORT, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = BSP_TOUCH_INT_PIN;
  HAL_GPIO_Init(BSP_TOUCH_INT_PORT, &GPIO_InitStruct);
}

static void BSP_Touch_ResetController(GPIO_PinState int_level)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  GPIO_InitStruct.Pin = BSP_TOUCH_INT_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(BSP_TOUCH_INT_PORT, &GPIO_InitStruct);

  HAL_GPIO_WritePin(BSP_TOUCH_INT_PORT, BSP_TOUCH_INT_PIN, int_level);
  HAL_GPIO_WritePin(BSP_TOUCH_RST_PORT, BSP_TOUCH_RST_PIN, GPIO_PIN_RESET);
  BSP_Touch_DelayShort();
  BSP_Touch_DelayShort();
  HAL_Delay(2);

  HAL_GPIO_WritePin(BSP_TOUCH_RST_PORT, BSP_TOUCH_RST_PIN, GPIO_PIN_SET);
  HAL_Delay(20);

  GPIO_InitStruct.Pin = BSP_TOUCH_INT_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(BSP_TOUCH_INT_PORT, &GPIO_InitStruct);
  HAL_Delay(60);
}

static HAL_StatusTypeDef BSP_Touch_Probe(uint8_t *product_id)
{
  if (BSP_Touch_ReadRegister(bsp_touch_i2c_address, BSP_TOUCH_REG_PRODUCT_ID, product_id, 6U) != HAL_OK)
  {
    bsp_touch_ready = 0U;
    g_bsp_touch_debug_status.probe_failures++;
    return HAL_ERROR;
  }

  if (((product_id[0] == 0x00U) && (product_id[1] == 0x00U) && (product_id[2] == 0x00U))
      || ((product_id[0] == 0xFFU) && (product_id[1] == 0xFFU) && (product_id[2] == 0xFFU)))
  {
    bsp_touch_ready = 0U;
    g_bsp_touch_debug_status.probe_failures++;
    return HAL_ERROR;
  }

  memcpy(g_bsp_touch_debug_status.product_id, product_id, 6U);
  g_bsp_touch_debug_status.active_address = bsp_touch_i2c_address;
  bsp_touch_ready = 1U;
  return HAL_OK;
}

static HAL_StatusTypeDef BSP_Touch_WriteDefaultConfig(void)
{
  uint8_t buffer[sizeof(bsp_touch_gt911_config) + 2U];
  uint32_t checksum = 0U;

  memcpy(buffer, bsp_touch_gt911_config, sizeof(bsp_touch_gt911_config));

  buffer[1] = (uint8_t)(BSP_DISPLAY_WIDTH & 0xFFU);
  buffer[2] = (uint8_t)((BSP_DISPLAY_WIDTH >> 8) & 0xFFU);
  buffer[3] = (uint8_t)(BSP_DISPLAY_HEIGHT & 0xFFU);
  buffer[4] = (uint8_t)((BSP_DISPLAY_HEIGHT >> 8) & 0xFFU);
  buffer[6] = (uint8_t)(buffer[6] | (1U << 3));

  for (uint32_t i = 0U; i < sizeof(bsp_touch_gt911_config); ++i)
  {
    checksum += buffer[i];
  }

  buffer[sizeof(bsp_touch_gt911_config)] = (uint8_t)((~checksum) + 1U);
  buffer[sizeof(bsp_touch_gt911_config) + 1U] = 0x01U;

  if (BSP_Touch_WriteRegister(bsp_touch_i2c_address,
                              BSP_TOUCH_REG_CONFIG,
                              buffer,
                              (uint16_t)sizeof(buffer)) != HAL_OK)
  {
    g_bsp_touch_debug_status.config_write_ok = 0U;
    return HAL_ERROR;
  }

  HAL_Delay(10);
  g_bsp_touch_debug_status.config_write_ok = 1U;
  return HAL_OK;
}

static void BSP_Touch_TransformCoordinates(int32_t *x, int32_t *y)
{
  int32_t tx = *x;
  int32_t ty = *y;

  if (BSP_TOUCH_SWAP_XY != 0U)
  {
    const int32_t temp = tx;
    tx = ty;
    ty = temp;
  }

  if (BSP_TOUCH_INVERT_X != 0U)
  {
    tx = (int32_t)BSP_DISPLAY_WIDTH - 1 - tx;
  }

  if (BSP_TOUCH_INVERT_Y != 0U)
  {
    ty = (int32_t)BSP_DISPLAY_HEIGHT - 1 - ty;
  }

  *x = tx;
  *y = ty;
}

HAL_StatusTypeDef BSP_Touch_Init(void)
{
  uint8_t product_id[6] = {0};

  g_bsp_touch_debug_status.init_attempted = 1U;

  if (bsp_touch_initialized != 0U)
  {
    return (bsp_touch_ready != 0U) ? HAL_OK : BSP_Touch_Probe(product_id);
  }

  BSP_Touch_ConfigurePins();
  memset(g_bsp_touch_debug_status.product_id, 0, sizeof(g_bsp_touch_debug_status.product_id));

  bsp_touch_i2c_address = BSP_TOUCH_I2C_ADDRESS_0;
  BSP_Touch_ResetController(GPIO_PIN_RESET);
  if (BSP_Touch_Probe(product_id) != HAL_OK)
  {
    bsp_touch_i2c_address = BSP_TOUCH_I2C_ADDRESS_1;
    BSP_Touch_ResetController(GPIO_PIN_SET);
    if (BSP_Touch_Probe(product_id) != HAL_OK)
    {
      bsp_touch_initialized = 1U;
      g_bsp_touch_debug_status.init_ok = 0U;
      return HAL_ERROR;
    }
  }

  (void)BSP_Touch_WriteDefaultConfig();
  bsp_touch_initialized = 1U;
  g_bsp_touch_debug_status.init_ok = 1U;
  return HAL_OK;
}

uint8_t BSP_Touch_IsReady(void)
{
  return (uint8_t)bsp_touch_ready;
}

uint8_t BSP_Touch_Read(int32_t *x, int32_t *y)
{
  uint8_t point_data[9] = {0};
  uint8_t clear_status = 0U;
  uint8_t status = 0U;
  uint8_t touch_count = 0U;
  int32_t touch_x = 0;
  int32_t touch_y = 0;
  const uint32_t now = HAL_GetTick();
  const uint32_t poll_interval = (bsp_touch_last_touch_active != 0U)
                                   ? BSP_TOUCH_ACTIVE_POLL_MS
                                   : BSP_TOUCH_IDLE_POLL_MS;

  if ((x == NULL) || (y == NULL))
  {
    return 0U;
  }

  if (bsp_touch_initialized == 0U)
  {
    (void)BSP_Touch_Init();
  }

  if ((bsp_touch_ready == 0U) && (BSP_Touch_Probe(g_bsp_touch_debug_status.product_id) != HAL_OK))
  {
    return 0U;
  }

  if ((now - bsp_touch_last_poll_tick) < poll_interval)
  {
    if (bsp_touch_last_touch_active != 0U)
    {
      *x = bsp_touch_cached_x;
      *y = bsp_touch_cached_y;
      return 1U;
    }
    return 0U;
  }

  bsp_touch_last_poll_tick = now;
  g_bsp_touch_debug_status.read_attempts++;
  if (BSP_Touch_ReadRegister(bsp_touch_i2c_address, BSP_TOUCH_REG_COORD, point_data, sizeof(point_data)) != HAL_OK)
  {
    bsp_touch_ready = 0U;
    bsp_touch_last_touch_active = 0U;
    g_bsp_touch_debug_status.read_failures++;
    return 0U;
  }

  status = point_data[0];
  g_bsp_touch_debug_status.last_status = status;
  if ((status & 0x80U) == 0U)
  {
    bsp_touch_last_touch_active = 0U;
    g_bsp_touch_debug_status.last_touch_count = 0U;
    return 0U;
  }

  g_bsp_touch_debug_status.ready_status_count++;
  g_bsp_touch_debug_status.last_nonzero_status = status;

  touch_count = (uint8_t)(status & 0x0FU);
  g_bsp_touch_debug_status.last_touch_count = touch_count;
  g_bsp_touch_debug_status.last_nonzero_touch_count = touch_count;
  if ((touch_count == 0U) || (touch_count > BSP_TOUCH_MAX_POINTS))
  {
    (void)BSP_Touch_WriteRegister(bsp_touch_i2c_address, BSP_TOUCH_REG_COORD, &clear_status, 1U);
    bsp_touch_last_touch_active = 0U;
    return 0U;
  }

  /*
   * GT911 returns:
   *   byte0  status
   *   byte1  track id
   *   byte2  x low
   *   byte3  x high
   *   byte4  y low
   *   byte5  y high
   */
  touch_x = (int32_t)point_data[2] | ((int32_t)point_data[3] << 8);
  touch_y = (int32_t)point_data[4] | ((int32_t)point_data[5] << 8);
  BSP_Touch_TransformCoordinates(&touch_x, &touch_y);

  (void)BSP_Touch_WriteRegister(bsp_touch_i2c_address, BSP_TOUCH_REG_COORD, &clear_status, 1U);

  if ((touch_x < 0) || (touch_y < 0)
      || (touch_x >= (int32_t)BSP_DISPLAY_WIDTH)
      || (touch_y >= (int32_t)BSP_DISPLAY_HEIGHT))
  {
    bsp_touch_last_touch_active = 0U;
    return 0U;
  }

  *x = touch_x;
  *y = touch_y;
  bsp_touch_cached_x = touch_x;
  bsp_touch_cached_y = touch_y;
  bsp_touch_last_touch_active = 1U;
  g_bsp_touch_debug_status.last_x = touch_x;
  g_bsp_touch_debug_status.last_y = touch_y;
  g_bsp_touch_debug_status.last_nonzero_x = touch_x;
  g_bsp_touch_debug_status.last_nonzero_y = touch_y;
  g_bsp_touch_debug_status.touch_seen_count++;
  return 1U;
}
