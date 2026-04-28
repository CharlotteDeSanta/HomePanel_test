#ifndef MAINVIEW_HPP
#define MAINVIEW_HPP

#include <gui_generated/main_screen/MainViewBase.hpp>
#include <gui/main_screen/MainPresenter.hpp>

class MainView : public MainViewBase
{
public:
    MainView();
    virtual ~MainView() {}

    virtual void setupScreen();
    virtual void tearDownScreen();
    virtual void handleTickEvent();
    virtual void kitchenCardButtonClicked();
    virtual void livingroomCardButtonClicked();
    virtual void bedroomCardButtonClicked();
    virtual void resetRoomCards();
    virtual void fanModeChanged(HVAC_FanMode_t fanMode);
    virtual void temperatureChanged(float temperature);
    virtual void temperatureAnimationDone();
    virtual void graphButtonClicked(Rooms room);

    void updateRoomTemperature(Rooms roomId, float temp);
    void updateRoomHumidity(Rooms roomId, float hum);
    void updateRoomFanMode(Rooms roomId, HVAC_FanMode_t fanMode);
    void updateClock(uint8_t hour, uint8_t minute);
    void updateDate(uint16_t year, uint8_t month, uint8_t day, uint8_t weekday);
    void updateCurrentWeather(WeatherData weatherData, bool isUnitFahrenheit);

private:
    template <typename T>
    void startupAnimation(FadeAnimator< T >& widget, uint16_t duration, uint16_t delay)
    {
        widget.setAlpha(0);
        widget.setFadeAnimationDelay(delay);
        widget.startFadeAnimation(255, duration, EasingEquations::quadEaseIn);
    }
    void setTemperatures();
    void setSelectedRoom(Rooms room);

    const uint8_t FADE_DURATION = 30;
    const uint8_t FLIP_DURATION = 20;
    const uint16_t CARD_X       = 495;
    const uint16_t LIVINGROOM_Y = 178;
    const uint16_t BEDROOM_Y    = 305;

    int tickCount;
    bool waitingToFlip;
};

#endif // MAINVIEW_HPP
