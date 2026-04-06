#ifndef BSP_DISPLAY_H
#define BSP_DISPLAY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "bsp_config.h"
#include "ltdc.h"

void BSP_Display_Init(void);
uint32_t *BSP_Display_GetFrameBuffer(void);
void BSP_Display_FillColor(uint32_t color);
void BSP_Display_FillTestPattern(void);
void BSP_Display_MspInit(void);
void BSP_Display_MspDeInit(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_DISPLAY_H */
