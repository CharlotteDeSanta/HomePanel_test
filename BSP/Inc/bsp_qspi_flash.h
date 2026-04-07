#ifndef BSP_QSPI_FLASH_H
#define BSP_QSPI_FLASH_H

#ifdef __cplusplus
extern "C" {
#endif

#include "bsp_config.h"

HAL_StatusTypeDef BSP_QSPI_Init(void);
HAL_StatusTypeDef BSP_QSPI_Read(uint8_t *data, uint32_t address, uint32_t size);
HAL_StatusTypeDef BSP_QSPI_EnableMemoryMappedMode(void);
void BSP_QSPI_MspInit(void);
void BSP_QSPI_MspDeInit(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_QSPI_FLASH_H */
