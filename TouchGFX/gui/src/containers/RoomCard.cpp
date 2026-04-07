#include <gui/containers/RoomCard.hpp>
#include <images/BitmapDatabase.hpp>

RoomCard::RoomCard() :
    animationDuration(0)
{

}

void RoomCard::initialize()
{
    roomNameText.setVisible(false);
    humidityText.setVisible(false);
    humidityDroplet.setVisible(false);
    cardDegrees.setVisible(false);
    cardTemp.setVisible(false);

    fanModeImage.setAlpha(0);

    Application::getInstance()->registerTimerWidget(this);
    RoomCardBase::initialize();
}

void RoomCard::handleTickEvent()
{
    if (initDelay > 0)
    {
        initDelay--;
    }
    else
    {
        float xAngle = cardBackgroundMapper.getXAngle();
        if (xAngle < 0 && initiated == true)
        {
            cardBackgroundMapper.invalidateContent();
            cardBackgroundMapper.setXAngle(xAngle + rotationSpeed);
            cardBackgroundMapper.invalidateContent();
        }
        else if (xAngle >= 0 && initiated)
        {
            initiated = false;

            cardBackgroundMapper.setVisible(false);
            cardBackgroundImage.setVisible(true);
            cardFadeOverlay.setAlpha(255);
            cardFadeOverlay.startFadeAnimation(0, animationDuration);
            fanModeImage.startFadeAnimation(255, animationDuration);
            cardBackgroundImage.invalidate();
        }
        else if (xAngle > -0.5 * PI && deinitiated)
        {
            cardBackgroundMapper.invalidateContent();
            cardBackgroundMapper.setXAngle(xAngle - rotationSpeed);
            cardBackgroundMapper.invalidateContent();
        }
        else if (xAngle <= -0.5 * PI && deinitiated == true)
        {
            deinitiated = false;
        }
    }

    // Start corner fan animation during flip-in animation
    if (fanModeImage.getAlpha() == 255 && cornerAnimation.getBitmap() == BITMAP_FAN_CORNER_00_ID)
    {
        cornerAnimation.setVisible(true);
        cornerAnimation.invalidate();
        cornerAnimation.startAnimation(false);
    }

    // Hide corner fan during flip-out animation
    if (cornerAnimation.isVisible() && cornerAnimation.getBitmap() == BITMAP_FAN_CORNER_00_ID && fanModeImage.getAlpha() < 200)
    {
        cornerAnimation.setVisible(false);
    }

    // Show text when card is flipped in
    if (!roomNameText.isVisible() && cardFadeOverlay.getAlpha() == 255)
    {
        roomNameText.setVisible(true);
        humidityText.setVisible(true);
        humidityDroplet.setVisible(true);
        cardDegrees.setVisible(true);
        cardTemp.setVisible(true);
    }

    // Hide text and overlay when card is flipping
    if (cardFadeOverlay.getAlpha() == 255 && cardBackgroundMapper.isVisible())
    {
        roomNameText.setVisible(false);
        humidityText.setVisible(false);
        humidityDroplet.setVisible(false);
        cardDegrees.setVisible(false);
        cardTemp.setVisible(false);

        cardFadeOverlay.setAlpha(0);

        cardBackgroundImage.invalidate();
    }
}

void RoomCard::startupAnimation(uint16_t duration, uint16_t sequenceNumber, TypedText roomName)
{
    // Set static data
    roomNameText.setTypedText(roomName);
    roomNameText.invalidate();

    // Setup start animation
    initDelay = uint16_t(sequenceNumber * duration);
    initiated = true;
    cardBackgroundMapper.setAngles(-0.5 * PI, 0, 0);
    rotationSpeed = float((0.5 * PI) / (duration / 2.0f));
    animationDuration = (uint8_t)duration;
    cornerAnimation.setBitmap(BITMAP_FAN_CORNER_00_ID);
    cardBackgroundImage.setVisible(false);
    cardBackgroundMapper.setVisible(true);
    cardBackgroundMapper.invalidate();
}

void RoomCard::setTemperature(float temp, bool isUnitFahrenheit)
{
    // Check if temperature is within valid range
    if (temp > -10 && temp < 40)
    {
        // Hide decimal when unit is fahrenheit and value >= 100
        if (isUnitFahrenheit && temp * 1.8f + 32 >= 100.0f)
        {
            Unicode::snprintf(cardTempBuffer, CARDTEMP_SIZE, "%d", int(temp * 1.8f + 32));
        }
        else
        {
            Unicode::snprintfFloat(cardTempBuffer, CARDTEMP_SIZE, "%#04.1f", isUnitFahrenheit ? temp * 1.8f + 32 : temp);
        }
    }
    // Set fallback characters
    else
    {
        Unicode::snprintf(cardTempBuffer, CARDTEMP_SIZE, "--.-");
    }

    cardTemp.invalidate();

    cardDegrees.setTypedText(TypedText(isUnitFahrenheit ? T_FAHRENHEITUNIT : T_CELSIUSUNIT));
    cardDegrees.invalidate();
}

void RoomCard::setFanSetPoint(HVAC_FanMode_t fanMode)
{
    switch (fanMode)
    {
    case HVAC_FAN_LOW:
        fanModeImage.setBitmap(BITMAP_FAN_SPEED_1_DOTS_ID);
        break;
    case HVAC_FAN_MED:
        fanModeImage.setBitmap(BITMAP_FAN_SPEED_2_DOTS_ID);
        break;
    case HVAC_FAN_HIGH:
        fanModeImage.setBitmap(BITMAP_FAN_SPEED_3_DOTS_ID);
        break;
    default:
        fanModeImage.setBitmap(BITMAP_ICON_AUTO_ACTIVE_ID);
        break;
    }

    fanModeImage.invalidate();
}

void RoomCard::setHumidity(float hum)
{
    if (hum >= 0 && hum <= 100)
    {
        Unicode::snprintfFloat(humidityTextBuffer, HUMIDITYTEXT_SIZE, "%.f", hum);
    }
    // Set fallback characters
    else
    {
        Unicode::snprintf(humidityTextBuffer, HUMIDITYTEXT_SIZE, "--");
    }
    humidityText.invalidate();
}

void RoomCard::flipOut(uint16_t duration)
{
    rotationSpeed = float((0.5 * PI) / (duration / 2.0f));
    deinitiated = true;
    initDelay = duration;

    cardBackgroundMapper.setVisible(true);
    cardBackgroundImage.setVisible(false);
    cornerAnimation.startAnimation(true);
    cardBackgroundMapper.invalidate();

    fanModeImage.startFadeAnimation(0, duration, EasingEquations::quadEaseIn);
    cardFadeOverlay.startFadeAnimation(255, duration, EasingEquations::quadEaseIn);
}
