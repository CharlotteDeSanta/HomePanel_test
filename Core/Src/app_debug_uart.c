#include "app_debug_uart.h"

#include <string.h>

#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"
#include "usart.h"

#define APP_DEBUG_UART_DEFAULT_TIMEOUT_MS 1000U

static SemaphoreHandle_t g_appDebugUartMutex = NULL;

static UART_HandleTypeDef *APP_DebugUart_GetHandle(void)
{
  return &huart1;
}

static uint8_t APP_DebugUart_IsInIsr(void)
{
  return (__get_IPSR() != 0U) ? 1U : 0U;
}

static BaseType_t APP_DebugUart_Lock(uint32_t timeoutMs)
{
#if (INCLUDE_xTaskGetSchedulerState == 1)
  BaseType_t schedulerState = xTaskGetSchedulerState();

  if ((schedulerState != taskSCHEDULER_RUNNING) ||
      (APP_DebugUart_IsInIsr() != 0U))
  {
    return pdFALSE;
  }
#endif

  if (g_appDebugUartMutex == NULL)
  {
    g_appDebugUartMutex = xSemaphoreCreateMutex();
    if (g_appDebugUartMutex == NULL)
    {
      return pdFALSE;
    }
  }

  if (xSemaphoreTake(g_appDebugUartMutex, pdMS_TO_TICKS(timeoutMs)) != pdTRUE)
  {
    return pdFALSE;
  }

  return pdTRUE;
}

static void APP_DebugUart_Unlock(void)
{
  if ((g_appDebugUartMutex != NULL) &&
#if (INCLUDE_xTaskGetSchedulerState == 1)
      (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) &&
#endif
      (APP_DebugUart_IsInIsr() == 0U))
  {
    (void)xSemaphoreGive(g_appDebugUartMutex);
  }
}

HAL_StatusTypeDef APP_DebugUart_Write(const uint8_t *data, uint16_t length, uint32_t timeout)
{
  UART_HandleTypeDef *const uart = APP_DebugUart_GetHandle();
  HAL_StatusTypeDef result = HAL_ERROR;
  BaseType_t locked = pdFALSE;

  if ((data == NULL) || (length == 0U))
  {
    return HAL_OK;
  }

  if (uart->Instance == NULL)
  {
    return HAL_ERROR;
  }

  locked = APP_DebugUart_Lock(timeout);
  result = HAL_UART_Transmit(uart, (uint8_t *)data, length, timeout);
  if (locked == pdTRUE)
  {
    APP_DebugUart_Unlock();
  }

  return result;
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
