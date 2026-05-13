#ifndef SETTEMPANDFAN_HPP
#define SETTEMPANDFAN_HPP

#include <gui_generated/containers/SetTempAndFanBase.hpp>

class SetTempAndFan : public SetTempAndFanBase
{
public:
    SetTempAndFan();
    virtual ~SetTempAndFan() {}

    virtual void initialize();
    virtual void handleTickEvent();
    virtual void startupAnimation();
    virtual void scrollWheelIsAnimating(bool value);
    virtual void showContainer(Rooms roomID, HVAC_FanMode_t fanMode, float temperature, bool isFahrenheit);
    virtual void closeButtonClicked();
    virtual void close();
    virtual void fadeIn();
    virtual void fadeOut();
    virtual void fanModeChanged();
    virtual void tempUpdated(float value);

    HVAC_FanMode_t getSelectedFanButton();

private:
    void usbButtonClicked(const touchgfx::AbstractButton& src);
    void applyUsbButtonStates();

    bool startupAnimationFlag;
    bool closeAnimationFlag;
    bool showFlag;
    uint16_t showDelay;
    int16_t closeDelay;
    Rooms openRoom;
    uint16_t idleTime;
    uint8_t moveRate;
    bool isInitialized;
    float newTemp;
    bool usbButtonStates[3][3];
    touchgfx::Callback<SetTempAndFan, const touchgfx::AbstractButton&> usbButtonCallback;
};

#endif // SETTEMPANDFAN_HPP
