#ifndef BSP_QSPI_DIAG_H
#define BSP_QSPI_DIAG_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    uint32_t magic;
    uint32_t cmd_rgb565_raw_match;
    uint32_t cmd_rgb565_swap16_match;
    uint32_t cmd_argb_raw_match;
    uint32_t cmd_argb_swap16_match;
    uint32_t cmd_argb_reverse32_match;
    uint32_t mm_enable_status;
    uint32_t mm_rgb565_raw_match;
    uint32_t mm_argb_raw_match;
    uint32_t cmd_rgb565_mismatch_index;
    uint32_t cmd_argb_mismatch_index;
    uint32_t mm_rgb565_mismatch_index;
    uint32_t mm_argb_mismatch_index;
    uint32_t cmd_rgb565_word0;
    uint32_t cmd_argb_word0;
    uint32_t mm_rgb565_word0;
    uint32_t mm_argb_word0;
    uint32_t mm_hal_error;
    uint32_t single1_rgb565_word0;
    uint32_t single2_rgb565_word0;
    uint32_t single1_argb_word0;
    uint32_t single2_argb_word0;
} BSP_QSPI_DiagnosticStatus;

extern BSP_QSPI_DiagnosticStatus g_bsp_qspi_diag_status;

void BSP_QSPI_DiagnosticDraw(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_QSPI_DIAG_H */
