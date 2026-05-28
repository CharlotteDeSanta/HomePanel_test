#include "app_debug_uart.h"

#include <string.h>

#include "SEGGER_RTT.h"

#define APP_DEBUG_UART_DEFAULT_TIMEOUT_MS 1000U

HAL_StatusTypeDef APP_DebugUart_Write(const uint8_t *data, uint16_t length, uint32_t timeout)
{
  unsigned int written = 0U;

  if ((data == NULL) || (length == 0U))
  {
    return HAL_OK;
  }

  (void)timeout;
  written = SEGGER_RTT_Write(0U, data, (unsigned int)length);
  (void)written;
  return HAL_OK;
}

HAL_StatusTypeDef APP_DebugUart_WriteString(const char *text)
{
  if (text == NULL)
  {
    return HAL_ERROR;
  }

  return APP_DebugUart_Write((const uint8_t *)text,
                             (uint16_t)strlen(text),
                             APP_DEBUG_UART_DEFAULT_TIMEOUT_MS);
}

HAL_StatusTypeDef APP_DebugUart_Read(uint8_t *data, uint16_t length, uint32_t timeout)
{
  if ((data == NULL) || (length == 0U))
  {
    return HAL_ERROR;
  }

  if (timeout == 0U)
  {
    const unsigned int readNow = SEGGER_RTT_Read(0U, data, (unsigned int)length);
    return (readNow == (unsigned int)length) ? HAL_OK : HAL_TIMEOUT;
  }

  {
    uint32_t startTick = HAL_GetTick();
    uint16_t offset = 0U;

    while (offset < length)
    {
      const unsigned int readNow = SEGGER_RTT_Read(0U,
                                                   &data[offset],
                                                   (unsigned int)(length - offset));
      if (readNow > 0U)
      {
        offset = (uint16_t)(offset + (uint16_t)readNow);
        continue;
      }

      if ((timeout != HAL_MAX_DELAY) && ((HAL_GetTick() - startTick) >= timeout))
      {
        return HAL_TIMEOUT;
      }
    }
  }

  return HAL_OK;
}

int __io_putchar(int ch)
{
  uint8_t bytes[2];
  uint16_t length = 0U;

  if (ch == '\n')
  {
    bytes[length++] = '\r';
  }
  bytes[length++] = (uint8_t)ch;

  if (APP_DebugUart_Write(bytes, length, APP_DEBUG_UART_DEFAULT_TIMEOUT_MS) != HAL_OK)
  {
    return -1;
  }

  return ch;
}

int __io_getchar(void)
{
  uint8_t byte = 0U;

  if (APP_DebugUart_Read(&byte, 1U, HAL_MAX_DELAY) != HAL_OK)
  {
    return -1;
  }

  return (int)byte;
}
