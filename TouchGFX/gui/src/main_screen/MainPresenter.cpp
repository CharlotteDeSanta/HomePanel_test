#include <gui/main_screen/MainView.hpp>
#include <gui/main_screen/MainPresenter.hpp>

/* Bind MainPresenter to concrete MainView instance. */
MainPresenter::MainPresenter(MainView& v)
    : view(v)
{

}

/* Lifecycle hook when Main screen presenter becomes active. */
void MainPresenter::activate()
{

}

/* Lifecycle hook when Main screen presenter is no longer active. */
void MainPresenter::deactivate()
{

}

/* Forward temperature updates from model listener to current view. */
void MainPresenter::updateTemperature(Rooms roomId, float temperature)
{
    view.updateRoomTemperature(roomId, temperature);
}

/* Forward humidity updates from model listener to current view. */
void MainPresenter::updateHumidity(Rooms roomId, float humidity)
{
    view.updateRoomHumidity(roomId, humidity);
}

/* Forward runtime fan mode updates from model listener to current view. */
void MainPresenter::updateFanMode(Rooms roomId, HVAC_FanMode_t fanMode)
{
    view.updateRoomFanMode(roomId, fanMode);
}

/* Forward USB flag state updates from model listener to current view. */
void MainPresenter::updateUsbFlags(Rooms roomId, uint8_t usbFlags)
{
    view.updateRoomUsbFlags(roomId, usbFlags);
}

/* Forward periodic clock updates from model listener to current view. */
void MainPresenter::updateClock(uint8_t clockHour, uint8_t clockMinute)
{
    view.updateClock(clockHour, clockMinute);
}

/* Forward date updates from model listener to current view. */
void MainPresenter::updateDate(uint16_t year, uint8_t month, uint8_t day, uint8_t weekday)
{
    view.updateDate(year, month, day, weekday);
}

/* Forward weather payload updates to view (kept for compatibility with base flow). */
void MainPresenter::updateWeatherData(WeatherData weatherData, bool isUnitFahrenheit)
{
    view.updateCurrentWeather(weatherData, isUnitFahrenheit);
}

/* Forward global WiFi online/offline status to main view indicator. */
void MainPresenter::updateWiFiOnline(bool online)
{
    view.updateWiFiOnline(online);
}

/* Forward per-room online/offline status to passive room indicators. */
void MainPresenter::updateRoomOnline(Rooms roomId, bool online)
{
    view.updateRoomOnline(roomId, online);
}

/* Query current temperature value for a room from model cache. */
float MainPresenter::getRoomTemperature(Rooms roomId)
{
    return model->getRoomTemperature(roomId);
}

/* Query current humidity value for a room from model cache. */
float MainPresenter::getRoomHumidity(Rooms roomId)
{
    return model->getRoomHumidity(roomId);
}

/* Query current runtime fan mode for a room from model cache. */
HVAC_FanMode_t MainPresenter::getRoomFanMode(Rooms roomId)
{
    return model->getRoomFanMode(roomId);
}

/* Query current USB output bitfield for a room from model cache. */
uint8_t MainPresenter::getRoomUsbFlags(Rooms roomId)
{
    return model->getRoomUsbFlags(roomId);
}

/* Forward user temperature setpoint intent from view to model/backend. */
void MainPresenter::setRoomTempSetPoint(Rooms roomId, float temperature)
{
    model->setRoomTempSetPoint(roomId, temperature);
}

/* Forward user fan setpoint intent from view to model/backend. */
void MainPresenter::setRoomFanSetPoint(Rooms roomId, HVAC_FanMode_t fanMode)
{
    model->setRoomFanSetPoint(roomId, fanMode);
}

/* Forward user USB toggle intent from view to model/backend. */
void MainPresenter::setRoomUsbFlags(Rooms roomId, uint8_t usbFlags)
{
    model->setRoomUsbFlags(roomId, usbFlags);
}

/* Read active display unit preference used by weather/main screen widgets. */
bool MainPresenter::getIsFahrenheit()
{
    return model->getIsFahrenheit();
}

/* Update display unit preference used by weather/main screen widgets. */
void MainPresenter::setIsFahrenheit(bool isFahrenheit)
{
    model->setIsFahrenheit(isFahrenheit);
}

/* Return currently selected room context for cross-screen actions. */
Rooms MainPresenter::getSelectedRoom()
{
    return model->getSelectedRoom();
}

/* Persist selected room context used by settings/graph/detail screens. */
void MainPresenter::setSelectedRoom(Rooms room)
{
    model->setSelectedRoom(room);
}

/* Read configured target temperature for a specific room. */
float MainPresenter::getRoomTempSetPoint(Rooms room)
{
    return model->getRoomTempSetPoint(room);
}

/* Read configured target fan mode (AUTO/manual) for a specific room. */
HVAC_FanMode_t MainPresenter::getRoomFanSetPoint(Rooms room)
{
    return model->getRoomFanSetPoint(room);
}

/* Fetch latest weather payload snapshot from model cache. */
WeatherData MainPresenter::getWeatherData()
{
    return model->getWeatherData();
}

/* Read current clock hour used by main view time label. */
uint8_t MainPresenter::getClockHour()
{
    return model->getClockHour();
}

/* Read current clock minute used by main view time label. */
uint8_t MainPresenter::getClockMinute()
{
    return model->getClockMinute();
}

/* Read current calendar year used by main view date label. */
uint16_t MainPresenter::getClockYear()
{
    return model->getClockYear();
}

/* Read current calendar month used by main view date label. */
uint8_t MainPresenter::getClockMonth()
{
    return model->getClockMonth();
}

/* Read current calendar day used by main view date label. */
uint8_t MainPresenter::getClockDay()
{
    return model->getClockDay();
}

/* Read current calendar weekday used by main view date label. */
uint8_t MainPresenter::getClockWeekday()
{
    return model->getClockWeekday();
}

/* Expose global WiFi online status for initial view-state binding. */
bool MainPresenter::getWiFiOnline()
{
    return model->getWiFiOnline();
}

/* Expose per-room online status for initial indicator binding. */
bool MainPresenter::getRoomOnline(Rooms roomId)
{
    return model->getRoomOnline(roomId);
}
