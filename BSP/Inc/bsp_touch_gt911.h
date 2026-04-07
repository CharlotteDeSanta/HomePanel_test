#ifndef BSP_TOUCH_GT911_H
#define BSP_TOUCH_GT911_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "main.h"

typedef struct
{
  uint32_t init_attempted;
  uint32_t init_ok;
  uint32_t config_write_ok;
  uint32_t active_address;
  uint32_t probe_failures;
  uint32_t read_failures;
  uint32_t read_attempts;
  uint32_t ready_status_count;
  uint32_t touch_seen_count;
  uint32_t last_status;
  uint32_t last_nonzero_status;
  uint32_t last_touch_count;
  uint32_t last_nonzero_touch_count;
  int32_t last_x;
  int32_t last_y;
  int32_t last_nonzero_x;
  int32_t last_nonzero_y;
  uint8_t product_id[6];
} BSP_Touch_DebugStatus;

extern BSP_Touch_DebugStatus g_bsp_touch_debug_status;

HAL_StatusTypeDef BSP_Touch_Init(void);
uint8_t BSP_Touch_IsReady(void);
uint8_t BSP_Touch_Read(int32_t *x, int32_t *y);

#ifdef __cplusplus
}
#endif

#endif /* BSP_TOUCH_GT911_H */
