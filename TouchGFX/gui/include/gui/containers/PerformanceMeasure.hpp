#ifndef PERFORMANCEMEASURE_HPP
#define PERFORMANCEMEASURE_HPP

#include <gui_generated/containers/PerformanceMeasureBase.hpp>

class PerformanceMeasure : public PerformanceMeasureBase
{
public:
    PerformanceMeasure();
    virtual ~PerformanceMeasure() {}

    virtual void initialize();

    void updateMCULoad();
    void countTheFrames();

private:
    uint16_t previousMCULoadPercentage;
    uint16_t currentMCULoadPercentage;

    uint16_t frameSkippedCounter;
    uint16_t frames;
    uint16_t fps;
};

#endif // PERFORMANCEMEASURE_HPP
