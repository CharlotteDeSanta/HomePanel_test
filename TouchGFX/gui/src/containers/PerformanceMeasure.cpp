#include <gui/containers/PerformanceMeasure.hpp>

PerformanceMeasure::PerformanceMeasure() :
    previousMCULoadPercentage(0),
    currentMCULoadPercentage(0),
    frameSkippedCounter(0),
    frames(0),
    fps(0)
{

}

void PerformanceMeasure::initialize()
{
    PerformanceMeasureBase::initialize();
}

void PerformanceMeasure::updateMCULoad()
{
    currentMCULoadPercentage = touchgfx::HAL::getInstance()->getMCULoadPct();

    // Only update the displayed number when it is different than the previous one
    if (currentMCULoadPercentage != previousMCULoadPercentage)
    {
        Unicode::snprintf(mcuLoadPctTextBuffer, MCULOADPCTTEXT_SIZE, "%d", currentMCULoadPercentage);
        mcuLoadPctText.invalidate();
    }
}

void PerformanceMeasure::countTheFrames()
{
    // This section uses the HAL layer to detect the LCD Refresh count and calculates the FPS if it is changed
    if (touchgfx::HAL::getInstance()->getLCDRefreshCount() > 1)
    {
        frameSkippedCounter++;
    }
    else
    {
        frames++;
    }

    // Update value if it has changed
    if (frames + frameSkippedCounter >= 60)
    {
        if (fps != frames)
        {
            fps = frames;
            Unicode::snprintf(framerateTextBuffer, FRAMERATETEXT_SIZE, "%d", fps);
            framerateText.invalidate();
        }
        frameSkippedCounter = 0;
        frames = 0;
    }
}
