#ifndef TEMPERATUREANIMATION_HPP
#define TEMPERATUREANIMATION_HPP

#include <gui_generated/containers/TemperatureAnimationBase.hpp>

class TemperatureAnimation : public TemperatureAnimationBase
{
public:
    TemperatureAnimation();
    virtual ~TemperatureAnimation() {}

    virtual void initialize();
    virtual void handleTickEvent();

    void startAnimation(Rooms selectedRoom, float tempSetPoint, float currentTemp, bool isUnitFahrenheit);
    void stopAnimation();
    void setTemperature(float tempSetPoint, bool isUnitFahrenheit);

private:
    int tickCount;

    const int ANIMATION_DURATION = 5 * 60;
    const uint8_t FADE_DURATION = 30;
};

#endif // TEMPERATUREANIMATION_HPP
