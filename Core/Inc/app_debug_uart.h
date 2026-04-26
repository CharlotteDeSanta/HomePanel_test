#ifndef APP_DEBUG_UART_H
#define APP_DEBUG_UART_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "stm32h7xx_hal.h"

HAL_StatusTypeDef APP_DebugUart_Write(const uint8_t *data, uint16_t length, uint32_t timeout);
HAL_StatusTypeDef APP_DebugUart_WriteString(const char *text);
HAL_StatusTypeDef APP_DebugUart_Read(uint8_t *data, uint16_t length, uint32_t timeout);

int __io_putchar(int ch);
int __io_getchar(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_DEBUG_UART_H */
