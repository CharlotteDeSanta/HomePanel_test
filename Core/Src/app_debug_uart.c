#include "app_debug_uart.h"

#include <string.h>

#include "usart.h"

#define APP_DEBUG_UART_DEFAULT_TIMEOUT_MS 1000U

static UART_HandleTypeDef *APP_DebugUart_GetHandle(void)
{
  return &huart1;
}

HAL_StatusTypeDef APP_DebugUart_Write(const uint8_t *data, uint16_t length, uint32_t timeout)
{
  UART_HandleTypeDef *const uart = APP_DebugUart_GetHandle();

  if ((data == NULL) || (length == 0U))
  {
    return HAL_OK;
  }

  if (uart->Instance == NULL)
  {
    return HAL_ERROR;
  }

  return HAL_UART_Transmit(uart, (uint8_t *)data, length, timeout);
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
  UART_HandleTypeDef *const uart = APP_DebugUart_GetHandle();

  if ((data == NULL) || (length == 0U))
  {
    return HAL_ERROR;
  }

  if (uart->Instance == NULL)
  {
    return HAL_ERROR;
  }

  return HAL_UART_Receive(uart, data, length, timeout);
}

int __io_putchar(int ch)
{
  uint8_t byte = (uint8_t)ch;

  if (ch == '\n')
  {
    const uint8_t carriageReturn = '\r';
    (void)APP_DebugUart_Write(&carriageReturn, 1U, APP_DEBUG_UART_DEFAULT_TIMEOUT_MS);
  }

  if (APP_DebugUart_Write(&byte, 1U, APP_DEBUG_UART_DEFAULT_TIMEOUT_MS) != HAL_OK)
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
