#include <gui/containers/TemperatureAnimation.hpp>
#include <texts/TextKeysAndLanguages.hpp>

TemperatureAnimation::TemperatureAnimation() :
    tickCount(0)
{

}

void TemperatureAnimation::initialize()
{
    Application::getInstance()->registerTimerWidget(this);
    TemperatureAnimationBase::initialize();
}

void TemperatureAnimation::handleTickEvent()
{
    tickCount++;

    if (tickCount == ANIMATION_DURATION && (temperatureUpAnimation.isAnimatedImageRunning() || temperatureDownAnimation.isAnimatedImageRunning()))
    {
        stopAnimation();
    }

    if ((temperatureUpAnimation.getAlpha() == 0 && temperatureDownAnimation.getAlpha() == 0) && (temperatureUpAnimation.isAnimatedImageRunning() || temperatureDownAnimation.isAnimatedImageRunning()))
    {
        temperatureUpAnimation.stopAnimation();
        temperatureDownAnimation.stopAnimation();
    }
}

void TemperatureAnimation::startAnimation(Rooms selectedRoom, float tempSetPoint, float currentTemp, bool isUnitFahrenheit)
{
    // Set position according to selected room
    switch (selectedRoom)
    {
    case KITCHEN:
        setXY(130, 180);
        break;
    case LIVINGROOM:
        setXY(340, 130);
        break;
    case BEDROOM:
        setXY(310, 315);
        break;
    default:
        break;
    }

    // Set animation according to temperature set
    if (tempSetPoint > currentTemp)
    {
        temperatureUpAnimation.setAlpha(1);
        temperatureUpAnimation.startFadeAnimation(255, FADE_DURATION);
        temperatureUpAnimation.startAnimation(false, true, true);
    }
    else
    {
        temperatureDownAnimation.setAlpha(1);
        temperatureDownAnimation.startFadeAnimation(255, FADE_DURATION);
        temperatureDownAnimation.startAnimation(false, true, true);
    }

    // Set temperature
    setTemperature(tempSetPoint, isUnitFahrenheit);
    temperatureText.startFadeAnimation(255, FADE_DURATION);

    tickCount = 0;
}

void TemperatureAnimation::stopAnimation()
{
    temperatureUpAnimation.startFadeAnimation(0, FADE_DURATION);
    temperatureDownAnimation.startFadeAnimation(0, FADE_DURATION);
    temperatureText.startFadeAnimation(0, FADE_DURATION);

    emitTemperatureAnimationDoneCallback();
}

void TemperatureAnimation::setTemperature(float tempSetPoint, bool isUnitFahrenheit)
{
    if (isUnitFahrenheit)
    {
        if (tempSetPoint * 1.8f + 32 >= 100.0f)
        {
            Unicode::snprintf(temperatureTextBuffer, TEMPERATURETEXT_SIZE, "%d", (int)(tempSetPoint * 1.8f + 32));
        }
        else
        {
            Unicode::snprintfFloat(temperatureTextBuffer, TEMPERATURETEXT_SIZE, "%.1f", tempSetPoint * 1.8f + 32);
        }

        temperatureText.setTypedText(T_TEMPANIMATIONF);
    }
    else
    {
        Unicode::snprintfFloat(temperatureTextBuffer, TEMPERATURETEXT_SIZE, "%.1f", tempSetPoint);
        temperatureText.setTypedText(T_TEMPANIMATIONC);
    }

    temperatureText.invalidate();
}
