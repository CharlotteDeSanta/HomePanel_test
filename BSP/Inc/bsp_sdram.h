#ifndef BSP_SDRAM_H
#define BSP_SDRAM_H

#ifdef __cplusplus
extern "C" {
#endif

#include "bsp_config.h"
#include "fmc.h"

void BSP_SDRAM_Init(void);
void BSP_SDRAM_Test(void);
void BSP_SDRAM_MspInit(void);
void BSP_SDRAM_MspDeInit(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_SDRAM_H */
