#ifndef BSP_BOARD_H
#define BSP_BOARD_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

void BSP_Board_Init(void);
void BSP_Board_SystemClock_Config(void);
void BSP_Board_ResetDisplay(void);
void BSP_Board_SetStatusLed(uint32_t red_on, uint32_t green_on, uint32_t blue_on);
void BSP_Board_MPU_Config(void);
void BSP_Board_FatalError(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_BOARD_H */
