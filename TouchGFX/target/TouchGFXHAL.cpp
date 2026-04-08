/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : TouchGFXHAL.cpp
  ******************************************************************************
  * This file was created by TouchGFX Generator 4.26.1. This file is only
  * generated once! Delete this file from your project and re-generate code
  * using STM32CubeMX or change this file manually to update it.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

#include <TouchGFXHAL.hpp>

/* USER CODE BEGIN TouchGFXHAL.cpp */
#include <touchgfx/hal/OSWrappers.hpp>

#include "bsp_config.h"
#include "ltdc.h"

using namespace touchgfx;

namespace
{
uint32_t getActiveAreaStartLine()
{
    return (LTDC->BPCR & LTDC_BPCR_AVBP_Msk) + 1U;
}

uint32_t getFrontPorchStartLine()
{
    return (LTDC->AWCR & LTDC_AWCR_AAH_Msk) + 1U;
}
} // namespace

void TouchGFXHAL::initialize()
{
    TouchGFXGeneratedHAL::initialize();

    /*
     * Use a second full-size SDRAM framebuffer so TouchGFX can render off-screen
     * and only swap the LTDC base address during VSync.
     */
    setFrameBufferStartAddresses((void*)BSP_DISPLAY_FRAMEBUFFER_ADDR,
                                 (void*)BSP_DISPLAY_FRAMEBUFFER1_ADDR,
                                 (void*)0);
}

/**
 * Gets the frame buffer address used by the TFT controller.
 *
 * @return The address of the frame buffer currently being displayed on the TFT.
 */
uint16_t* TouchGFXHAL::getTFTFrameBuffer() const
{
    return reinterpret_cast<uint16_t*>(LTDC_Layer1->CFBAR);
}

/**
 * Sets the frame buffer address used by the TFT controller.
 *
 * @param [in] address New frame buffer address.
 */
void TouchGFXHAL::setTFTFrameBuffer(uint16_t* address)
{
    /*
     * The address swap is requested from the LTDC line-event callback exactly
     * at the frame boundary, so an immediate reload applies the new buffer to
     * the frame that is about to start.
     */
    HAL_LTDC_SetAddress_NoReload(&hltdc, reinterpret_cast<uint32_t>(address), 0);
    HAL_LTDC_Reload(&hltdc, LTDC_RELOAD_IMMEDIATE);
}

/**
 * This function is called whenever the framework has performed a partial draw.
 *
 * @param rect The area of the screen that has been drawn, expressed in absolute coordinates.
 *
 * @see flushFrameBuffer().
 */
void TouchGFXHAL::flushFrameBuffer(const touchgfx::Rect& rect)
{
    // Calling parent implementation of flushFrameBuffer(const touchgfx::Rect& rect).
    //
    // To overwrite the generated implementation, omit the call to the parent function
    // and implement the needed functionality here.
    // Please note, HAL::flushFrameBuffer(const touchgfx::Rect& rect) must
    // be called to notify the touchgfx framework that flush has been performed.
    // To calculate the start address of rect,
    // use advanceFrameBufferToRect(uint8_t* fbPtr, const touchgfx::Rect& rect)
    // defined in TouchGFXGeneratedHAL.cpp

    TouchGFXGeneratedHAL::flushFrameBuffer(rect);
}

bool TouchGFXHAL::blockCopy(void* RESTRICT dest, const void* RESTRICT src, uint32_t numBytes)
{
    return TouchGFXGeneratedHAL::blockCopy(dest, src, numBytes);
}

/**
 * Configures the interrupts relevant for TouchGFX. This primarily entails setting
 * the interrupt priorities for the DMA and LCD interrupts.
 */
void TouchGFXHAL::configureInterrupts()
{
    // Calling parent implementation of configureInterrupts().
    //
    // To overwrite the generated implementation, omit the call to the parent function
    // and implement the needed functionality here.

    TouchGFXGeneratedHAL::configureInterrupts();
}

/**
 * Used for enabling interrupts set in configureInterrupts()
 */
void TouchGFXHAL::enableInterrupts()
{
    // Calling parent implementation of enableInterrupts().
    //
    // To overwrite the generated implementation, omit the call to the parent function
    // and implement the needed functionality here.

    TouchGFXGeneratedHAL::enableInterrupts();
}

/**
 * Used for disabling interrupts set in configureInterrupts()
 */
void TouchGFXHAL::disableInterrupts()
{
    // Calling parent implementation of disableInterrupts().
    //
    // To overwrite the generated implementation, omit the call to the parent function
    // and implement the needed functionality here.

    TouchGFXGeneratedHAL::disableInterrupts();
}

/**
 * Configure the LCD controller to fire interrupts at VSYNC. Called automatically
 * once TouchGFX initialization has completed.
 */
void TouchGFXHAL::enableLCDControllerInterrupt()
{
    HAL_LTDC_ProgramLineEvent(&hltdc, getActiveAreaStartLine());
}

bool TouchGFXHAL::beginFrame()
{
    return TouchGFXGeneratedHAL::beginFrame();
}

void TouchGFXHAL::endFrame()
{
    TouchGFXGeneratedHAL::endFrame();
}

extern "C" void HAL_LTDC_LineEventCallback(LTDC_HandleTypeDef* hltdcHandle)
{
    if (hltdcHandle->Instance != LTDC || !HAL::getInstance())
    {
        return;
    }

    if (LTDC->LIPCR == getActiveAreaStartLine())
    {
        HAL_LTDC_ProgramLineEvent(hltdcHandle, getFrontPorchStartLine());
        HAL::getInstance()->vSync();
        OSWrappers::signalVSync();
        HAL::getInstance()->swapFrameBuffers();
    }
    else
    {
        HAL_LTDC_ProgramLineEvent(hltdcHandle, getActiveAreaStartLine());
        HAL::getInstance()->frontPorchEntered();
    }
}

/* USER CODE END TouchGFXHAL.cpp */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
