#ifndef MAINPRESENTER_HPP
#define MAINPRESENTER_HPP

#include <gui/model/ModelListener.hpp>
#include <mvp/Presenter.hpp>

using namespace touchgfx;

class MainView;

class MainPresenter : public touchgfx::Presenter, public ModelListener
{
public:
    MainPresenter(MainView& v);

    /**
     * The activate function is called automatically when this screen is "switched in"
     * (ie. made active). Initialization logic can be placed here.
     */
    virtual void activate();

    /**
     * The deactivate function is called automatically when this screen is "switched out"
     * (ie. made inactive). Teardown functionality can be placed here.
     */
    virtual void deactivate();

    virtual ~MainPresenter() {}

    virtual void updateTemperature(Rooms roomId, float temperature);
    virtual void updateHumidity(Rooms roomId, float humidity);
    virtual void updateFanMode(Rooms roomId, HVAC_FanMode_t fanMode);
    virtual void updateClock(uint8_t clockHour, uint8_t clockMinute);
    virtual void updateDate(uint16_t year, uint8_t month, uint8_t day, uint8_t weekday);
    virtual void updateWeatherData(WeatherData weatherData, bool isUnitFahrenheit);

    float getRoomTemperature(Rooms roomId);
    float getRoomHumidity(Rooms roomId);
    HVAC_FanMode_t getRoomFanMode(Rooms roomId);
    void setRoomTempSetPoint(Rooms roomId, float temperature);
    void setRoomFanSetPoint(Rooms roomId, HVAC_FanMode_t fanMode);
    bool getIsFahrenheit();
    void setIsFahrenheit(bool isFahrenheit);
    Rooms getSelectedRoom();
    void setSelectedRoom(Rooms room);
    float getRoomTempSetPoint(Rooms room);
    HVAC_FanMode_t getRoomFanSetPoint(Rooms room);
    WeatherData getWeatherData();
    uint8_t getClockHour();
    uint8_t getClockMinute();
    uint16_t getClockYear();
    uint8_t getClockMonth();
    uint8_t getClockDay();
    uint8_t getClockWeekday();

private:
    MainPresenter();

    MainView& view;
};

#endif // MAINPRESENTER_HPP
