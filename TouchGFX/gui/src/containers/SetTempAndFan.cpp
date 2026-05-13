#include <gui/containers/SetTempAndFan.hpp>
#include <texts/TextKeysAndLanguages.hpp>
#include <images/BitmapDatabase.hpp>

SetTempAndFan::SetTempAndFan() :
    startupAnimationFlag(false),
    closeAnimationFlag(false),
    showFlag(false),
    showDelay(0),
    closeDelay(0),
    openRoom(KITCHEN),
    idleTime(0),
    moveRate(12),
    isInitialized(false),
    newTemp(0),
    usbButtonStates{},
    usbButtonCallback(this, &SetTempAndFan::usbButtonClicked)
{

}

void SetTempAndFan::initialize()
{
    Application::getInstance()->registerTimerWidget(this);
    SetTempAndFanBase::initialize();

    usbButton1.setAction(usbButtonCallback);
    usbButton2.setAction(usbButtonCallback);
    usbButton3.setAction(usbButtonCallback);
}

void SetTempAndFan::startupAnimation()
{
    // Hide all elements and move to minimal size
    headerImage.setAlpha(0);
    graphButton.setAlpha(0);
    fanSpeedText.setAlpha(0);
    fanImage.setAlpha(0);
    autoFanButton.setAlpha(0);
    highFanButton.setAlpha(0);
    mediumFanButton.setAlpha(0);
    lowFanButton.setAlpha(0);
    headerText.setAlpha(0);
    usbButton1.setAlpha(0);
    usbButton2.setAlpha(0);
    usbButton3.setAlpha(0);
    usbButton1.setTouchable(false);
    usbButton2.setTouchable(false);
    usbButton3.setTouchable(false);
    scrollWheel1.setVisible(false);

    // Move into position
    bottomImage.setY(110 - 15);
    backgroundBox.setHeight(bottomImage.getY() - backgroundBox.getY());

    // Set the animation
    startupAnimationFlag = true;
}

void SetTempAndFan::handleTickEvent()
{
    if (showFlag)
    {
        if (showDelay == 0)
        {
            setVisible(true);
            startupAnimation();
            invalidate();
            showFlag = false;
        }
        showDelay--;
    }

    if (startupAnimationFlag)
    {
        if (bottomImage.getY() < (getHeight() - bottomImage.getHeight()))
        {
            bottomImage.moveRelative(0, moveRate);
            backgroundBox.setHeight(bottomImage.getY() - backgroundBox.getY());
        }
        else
        {
            invalidate();
            startupAnimationFlag = false;
            fadeIn();
            emitOpenCompleteCallback();
        }
    }

    if (closeAnimationFlag)
    {
        if (closeDelay > 0)
        {
            closeDelay--;
        }
        if (closeDelay == 0)
        {
            headerImage.setAlpha(0);
            graphButton.setAlpha(0);
            fanSpeedText.setAlpha(0);
            fanImage.setAlpha(0);
            autoFanButton.setAlpha(0);
            highFanButton.setAlpha(0);
            mediumFanButton.setAlpha(0);
            lowFanButton.setAlpha(0);
            headerText.setAlpha(0);
            usbButton1.setAlpha(0);
            usbButton2.setAlpha(0);
            usbButton3.setAlpha(0);
            usbButton1.setTouchable(false);
            usbButton2.setTouchable(false);
            usbButton3.setTouchable(false);
            scrollWheel1.setVisible(false);
            faderBox.setAlpha(0);

            faderBox.invalidate();

            // Make this happen only once
            closeDelay = -1;
        }
        if (bottomImage.getY() > 110 && closeDelay == -1)
        {
            bottomImage.moveRelative(0, -moveRate);
            backgroundBox.setHeight(bottomImage.getY() - backgroundBox.getY());
        }
        else if (bottomImage.getY() <= 110)
        {
            closeAnimationFlag = false;
            setVisible(false);
            invalidate();
            emitCloseCompleteCallback();
        }
    }

    if (isVisible())
    {
        idleTime++;
    }

    if (idleTime > 400)
    {
        idleTime = 0;
        close();
    }
}

void SetTempAndFan::scrollWheelIsAnimating(bool value)
{
    if (value == true)
    {
        idleTime = 0;
    }
}

void SetTempAndFan::showContainer(Rooms roomID, HVAC_FanMode_t fanMode, float temperature, bool isFahrenheit)
{
    openRoom = roomID;
    idleTime = 0;
    isInitialized = false;
    newTemp = temperature;

    scrollWheel1.setTempValue(temperature, isFahrenheit);

    switch (roomID)
    {
    case KITCHEN:
        showDelay = 30;
        headerText.setTypedText(T_KITCHENTEXTSMALL);
        headerImage.setBitmap(BITMAP_ICON_KITCHEN_ID);
        break;
    case LIVINGROOM:
        showDelay = 39;
        headerText.setTypedText(T_LIVINGROOMTEXTSMALL);
        headerImage.setBitmap(BITMAP_ICON_LIVINGROOM_ID);
        break;
    case BEDROOM:
        showDelay = 47;
        headerText.setTypedText(T_BEDROOMTEXTSMALL);
        headerImage.setBitmap(BITMAP_ICON_BEDROOM_ID);
        break;
    default:
        break;
    }
    headerText.invalidate();
    headerImage.invalidate();
    applyUsbButtonStates();

    showFlag = true;

    // Set selected fan button
    switch (fanMode)
    {
    case HVAC_FAN_LOW:
        fanButtons.setSelected(lowFanButton);
        break;
    case HVAC_FAN_MED:
        fanButtons.setSelected(mediumFanButton);
        break;
    case HVAC_FAN_HIGH:
        fanButtons.setSelected(highFanButton);
        break;
    default:    // Set as auto if other
        fanButtons.setSelected(autoFanButton);
        break;
    }

    isInitialized = true;
}

void SetTempAndFan::closeButtonClicked()
{
    close();
}

void SetTempAndFan::close()
{
    emitCloseStartedCallback(newTemp);
    switch (openRoom)
    {
    case KITCHEN:
        showDelay = 30;
        break;

    case LIVINGROOM:
        showDelay = 60;
        break;

    case BEDROOM:
        showDelay = 90;
        break;

    default:
        break;
    }
    fadeOut();
    closeDelay = 31;
    closeAnimationFlag = true;
}

void SetTempAndFan::fadeIn()
{
    faderBox.setAlpha(0);
    headerImage.setAlpha(255);
    graphButton.setAlpha(255);
    fanSpeedText.setAlpha(255);
    fanImage.setAlpha(255);
    autoFanButton.setAlpha(255);
    highFanButton.setAlpha(255);
    mediumFanButton.setAlpha(255);
    lowFanButton.setAlpha(255);
    headerText.setAlpha(255);
    usbButton1.setAlpha(255);
    usbButton2.setAlpha(255);
    usbButton3.setAlpha(255);
    usbButton1.setTouchable(true);
    usbButton2.setTouchable(true);
    usbButton3.setTouchable(true);
    scrollWheel1.setVisible(true);
    invalidate();
}

void SetTempAndFan::fadeOut()
{
    faderBox.setAlpha(255);
    faderBox.invalidate();
}

void SetTempAndFan::fanModeChanged()
{
    if (isInitialized)
    {
        idleTime = 0;
        emitFanSettingChangedCallback(getSelectedFanButton());
    }
}

void SetTempAndFan::tempUpdated(float value)
{
    idleTime = 0;
    newTemp = value;
}

void SetTempAndFan::usbButtonClicked(const touchgfx::AbstractButton& src)
{
    idleTime = 0;

    const uint8_t roomIndex = static_cast<uint8_t>(openRoom);
    if (roomIndex >= 3)
    {
        return;
    }

    if (&src == &usbButton1)
    {
        usbButtonStates[roomIndex][0] = usbButton1.getState();
    }
    else if (&src == &usbButton2)
    {
        usbButtonStates[roomIndex][1] = usbButton2.getState();
    }
    else if (&src == &usbButton3)
    {
        usbButtonStates[roomIndex][2] = usbButton3.getState();
    }
}

void SetTempAndFan::applyUsbButtonStates()
{
    const uint8_t roomIndex = static_cast<uint8_t>(openRoom);
    if (roomIndex >= 3)
    {
        return;
    }

    usbButton1.forceState(usbButtonStates[roomIndex][0]);
    usbButton2.forceState(usbButtonStates[roomIndex][1]);
    usbButton3.forceState(usbButtonStates[roomIndex][2]);

    usbButton1.invalidate();
    usbButton2.invalidate();
    usbButton3.invalidate();
}

HVAC_FanMode_t SetTempAndFan::getSelectedFanButton()
{
    switch (fanButtons.getSelectedRadioButtonIndex())
    {
    case 0:
        return HVAC_FAN_LOW;
        break;
    case 1:
        return HVAC_FAN_MED;
        break;
    case 2:
        return HVAC_FAN_HIGH;
        break;
    default:
        return HVAC_FAN_AUTO;
        break;
    }
}
