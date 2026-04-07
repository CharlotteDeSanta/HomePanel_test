#include "bsp_qspi_diag.h"

#include <cstddef>
#include <cstdint>

#include "bsp_config.h"
#include "bsp_display.h"
#include "bsp_qspi_flash.h"

extern const unsigned char image_bedroom_selected[];
extern const unsigned char image_close_normal[];

namespace
{
constexpr size_t kCompareSize = 32;
constexpr uint32_t kDiagSkipped = 0xDDDDDDDDUL;

constexpr uint8_t kExpectedRgb565[kCompareSize] = {
    0xad, 0x21, 0xad, 0x21, 0xae, 0x21, 0xae, 0x21,
    0xae, 0x21, 0xae, 0x21, 0xae, 0x21, 0xae, 0x21,
    0xcf, 0x21, 0xaf, 0x21, 0xaf, 0x21, 0xcf, 0x21,
    0xcf, 0x21, 0xcf, 0x21, 0xcf, 0x21, 0xcf, 0x21
};

constexpr uint8_t kExpectedArgb[kCompareSize] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0xf8, 0xfc, 0xf8, 0x04,
    0xf8, 0xfc, 0xf8, 0x55, 0xf8, 0xf8, 0xf8, 0xa6
};

uint8_t rgb565Bytes[kCompareSize];
uint8_t argbBytes[kCompareSize];

static uint32_t compareRaw(const uint8_t* actual, const uint8_t* expected, size_t count)
{
    for (size_t i = 0; i < count; ++i)
    {
        if (actual[i] != expected[i])
        {
            return static_cast<uint32_t>(i);
        }
    }
    return 0xFFFFFFFFUL;
}

static uint32_t readWord0(const uint8_t* data)
{
    return static_cast<uint32_t>(data[0])
         | (static_cast<uint32_t>(data[1]) << 8)
         | (static_cast<uint32_t>(data[2]) << 16)
         | (static_cast<uint32_t>(data[3]) << 24);
}

static uint32_t compareSwap16(const uint8_t* actual, const uint8_t* expected, size_t count)
{
    for (size_t i = 0; i < count; i += 2)
    {
        if ((actual[i] != expected[i + 1]) || (actual[i + 1] != expected[i]))
        {
            return static_cast<uint32_t>(i);
        }
    }
    return 0xFFFFFFFFUL;
}

static uint32_t compareReverse32(const uint8_t* actual, const uint8_t* expected, size_t count)
{
    for (size_t i = 0; i < count; i += 4)
    {
        if ((actual[i] != expected[i + 3]) ||
            (actual[i + 1] != expected[i + 2]) ||
            (actual[i + 2] != expected[i + 1]) ||
            (actual[i + 3] != expected[i]))
        {
            return static_cast<uint32_t>(i);
        }
    }
    return 0xFFFFFFFFUL;
}

static void drawBorder(uint32_t* fb, int x, int y, int w, int h, uint32_t color)
{
    for (int ix = 0; ix < w; ++ix)
    {
        fb[(y * BSP_DISPLAY_WIDTH) + (x + ix)] = color;
        fb[((y + h - 1) * BSP_DISPLAY_WIDTH) + (x + ix)] = color;
    }

    for (int iy = 0; iy < h; ++iy)
    {
        fb[((y + iy) * BSP_DISPLAY_WIDTH) + x] = color;
        fb[((y + iy) * BSP_DISPLAY_WIDTH) + (x + w - 1)] = color;
    }
}

static void drawRect(uint32_t* fb, int x0, int y0, int w, int h, uint32_t color)
{
    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            fb[((y0 + y) * BSP_DISPLAY_WIDTH) + (x0 + x)] = color;
        }
    }
}

static void drawColorSwatches(uint32_t* fb)
{
    static const uint32_t colors[] = {
        0xFFFF0000UL,
        0xFF00FF00UL,
        0xFF0000FFUL,
        0xFFFFFFFFUL
    };

    for (int i = 0; i < 4; ++i)
    {
        const int x0 = 24 + (i * 72);
        const int y0 = 360;
        drawRect(fb, x0, y0, 48, 48, colors[i]);
        drawBorder(fb, x0 - 2, y0 - 2, 52, 52, 0xFFFFFFFFUL);
    }
}

static void drawStatusBox(uint32_t* fb, int x0, int y0, uint32_t borderColor, uint32_t pass)
{
    const uint32_t fill = (pass != 0U) ? 0xFF18C04BUL : 0xFFC5352BUL;
    drawRect(fb, x0, y0, 60, 60, fill);
    drawBorder(fb, x0 - 2, y0 - 2, 64, 64, borderColor);
}
}

BSP_QSPI_DiagnosticStatus g_bsp_qspi_diag_status = {0};

extern "C" void BSP_QSPI_DiagnosticDraw(void)
{
    uint32_t* fb = BSP_Display_GetFrameBuffer();
    const uint32_t rgb565Offset =
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(image_bedroom_selected) - BSP_QSPI_BASE_ADDR);
    const uint32_t argb8888Offset =
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(image_close_normal) - BSP_QSPI_BASE_ADDR);

    g_bsp_qspi_diag_status = {};
    g_bsp_qspi_diag_status.magic = 0x51444947UL;

    for (uint32_t y = 0; y < BSP_DISPLAY_HEIGHT; ++y)
    {
        for (uint32_t x = 0; x < BSP_DISPLAY_WIDTH; ++x)
        {
            fb[(y * BSP_DISPLAY_WIDTH) + x] = 0xFF1A1F28UL;
        }
    }
    drawColorSwatches(fb);

    if (BSP_QSPI_Read(rgb565Bytes, rgb565Offset, sizeof(rgb565Bytes)) == HAL_OK)
    {
        g_bsp_qspi_diag_status.cmd_rgb565_word0 = readWord0(rgb565Bytes);
        g_bsp_qspi_diag_status.cmd_rgb565_mismatch_index =
            compareRaw(rgb565Bytes, kExpectedRgb565, sizeof(kExpectedRgb565));
        g_bsp_qspi_diag_status.cmd_rgb565_raw_match =
            (g_bsp_qspi_diag_status.cmd_rgb565_mismatch_index == 0xFFFFFFFFUL) ? 1U : 0U;
        g_bsp_qspi_diag_status.cmd_rgb565_swap16_match =
            (compareSwap16(rgb565Bytes, kExpectedRgb565, sizeof(kExpectedRgb565)) == 0xFFFFFFFFUL) ? 1U : 0U;
    }
    else
    {
        g_bsp_qspi_diag_status.cmd_rgb565_mismatch_index = 0xEEEEEEEEUL;
    }

    if (BSP_QSPI_Read(argbBytes, argb8888Offset, sizeof(argbBytes)) == HAL_OK)
    {
        g_bsp_qspi_diag_status.cmd_argb_word0 = readWord0(argbBytes);
        g_bsp_qspi_diag_status.cmd_argb_mismatch_index =
            compareRaw(argbBytes, kExpectedArgb, sizeof(kExpectedArgb));
        g_bsp_qspi_diag_status.cmd_argb_raw_match =
            (g_bsp_qspi_diag_status.cmd_argb_mismatch_index == 0xFFFFFFFFUL) ? 1U : 0U;
        g_bsp_qspi_diag_status.cmd_argb_swap16_match =
            (compareSwap16(argbBytes, kExpectedArgb, sizeof(kExpectedArgb)) == 0xFFFFFFFFUL) ? 1U : 0U;
        g_bsp_qspi_diag_status.cmd_argb_reverse32_match =
            (compareReverse32(argbBytes, kExpectedArgb, sizeof(kExpectedArgb)) == 0xFFFFFFFFUL) ? 1U : 0U;
    }
    else
    {
        g_bsp_qspi_diag_status.cmd_argb_mismatch_index = 0xEEEEEEEEUL;
    }

    g_bsp_qspi_diag_status.single1_rgb565_word0 = kDiagSkipped;
    g_bsp_qspi_diag_status.single2_rgb565_word0 = kDiagSkipped;
    g_bsp_qspi_diag_status.single1_argb_word0 = kDiagSkipped;
    g_bsp_qspi_diag_status.single2_argb_word0 = kDiagSkipped;

    g_bsp_qspi_diag_status.mm_enable_status =
        (BSP_QSPI_EnableMemoryMappedMode() == HAL_OK) ? 1U : 0U;
    g_bsp_qspi_diag_status.mm_hal_error = BSP_QSPI_GetError();

    if (g_bsp_qspi_diag_status.mm_enable_status != 0U)
    {
        const uint8_t* mmRgb = reinterpret_cast<const uint8_t*>(image_bedroom_selected);
        const uint8_t* mmArgb = reinterpret_cast<const uint8_t*>(image_close_normal);

        g_bsp_qspi_diag_status.mm_rgb565_word0 = readWord0(mmRgb);
        g_bsp_qspi_diag_status.mm_argb_word0 = readWord0(mmArgb);

        g_bsp_qspi_diag_status.mm_rgb565_mismatch_index =
            compareRaw(mmRgb, kExpectedRgb565, sizeof(kExpectedRgb565));
        g_bsp_qspi_diag_status.mm_rgb565_raw_match =
            (g_bsp_qspi_diag_status.mm_rgb565_mismatch_index == 0xFFFFFFFFUL) ? 1U : 0U;

        g_bsp_qspi_diag_status.mm_argb_mismatch_index =
            compareRaw(mmArgb, kExpectedArgb, sizeof(kExpectedArgb));
        g_bsp_qspi_diag_status.mm_argb_raw_match =
            (g_bsp_qspi_diag_status.mm_argb_mismatch_index == 0xFFFFFFFFUL) ? 1U : 0U;
    }
    else
    {
        g_bsp_qspi_diag_status.mm_rgb565_mismatch_index = 0xEEEEEEEEUL;
        g_bsp_qspi_diag_status.mm_argb_mismatch_index = 0xEEEEEEEEUL;
    }

    drawStatusBox(fb, 24, 32, 0xFFFFFFFFUL, g_bsp_qspi_diag_status.cmd_rgb565_raw_match);
    drawStatusBox(fb, 104, 32, 0xFFFFFF00UL, g_bsp_qspi_diag_status.cmd_rgb565_swap16_match);
    drawStatusBox(fb, 184, 32, 0xFFFFFFFFUL, g_bsp_qspi_diag_status.cmd_argb_raw_match);
    drawStatusBox(fb, 264, 32, 0xFFFFFF00UL, g_bsp_qspi_diag_status.cmd_argb_swap16_match);
    drawStatusBox(fb, 344, 32, 0xFF00FFFFUL, g_bsp_qspi_diag_status.cmd_argb_reverse32_match);
    drawStatusBox(fb, 24, 124, 0xFFFFFFFFUL, g_bsp_qspi_diag_status.mm_rgb565_raw_match);
    drawStatusBox(fb, 104, 124, 0xFFFFFFFFUL, g_bsp_qspi_diag_status.mm_argb_raw_match);
    drawStatusBox(fb, 184, 124, 0xFFFF00FFUL, g_bsp_qspi_diag_status.mm_enable_status);

    if ((SCB->CCR & SCB_CCR_DC_Msk) != 0U)
    {
        SCB_CleanInvalidateDCache();
    }
}
