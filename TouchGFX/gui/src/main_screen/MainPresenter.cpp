#include <gui/main_screen/MainView.hpp>
#include <gui/main_screen/MainPresenter.hpp>

MainPresenter::MainPresenter(MainView& v)
    : view(v)
{

}

void MainPresenter::activate()
{

}

void MainPresenter::deactivate()
{

}

void MainPresenter::updateTemperature(Rooms roomId, float temperature)
{
    view.updateRoomTemperature(roomId, temperature);
}

void MainPresenter::updateHumidity(Rooms roomId, float humidity)
{
    view.updateRoomHumidity(roomId, humidity);
}

void MainPresenter::updateFanMode(Rooms roomId, HVAC_FanMode_t fanMode)
{
    view.updateRoomFanMode(roomId, fanMode);
}

void MainPresenter::updateUsbFlags(Rooms roomId, uint8_t usbFlags)
{
    view.updateRoomUsbFlags(roomId, usbFlags);
}

void MainPresenter::updateClock(uint8_t clockHour, uint8_t clockMinute)
{
    view.updateClock(clockHour, clockMinute);
}

void MainPresenter::updateDate(uint16_t year, uint8_t month, uint8_t day, uint8_t weekday)
{
    view.updateDate(year, month, day, weekday);
}

void MainPresenter::updateWeatherData(WeatherData weatherData, bool isUnitFahrenheit)
{
    view.updateCurrentWeather(weatherData, isUnitFahrenheit);
}

void MainPresenter::updateWiFiOnline(bool online)
{
    view.updateWiFiOnline(online);
}

void MainPresenter::updateRoomOnline(Rooms roomId, bool online)
{
    view.updateRoomOnline(roomId, online);
}

float MainPresenter::getRoomTemperature(Rooms roomId)
{
    return model->getRoomTemperature(roomId);
}

float MainPresenter::getRoomHumidity(Rooms roomId)
{
    return model->getRoomHumidity(roomId);
}

HVAC_FanMode_t MainPresenter::getRoomFanMode(Rooms roomId)
{
    return model->getRoomFanMode(roomId);
}

uint8_t MainPresenter::getRoomUsbFlags(Rooms roomId)
{
    return model->getRoomUsbFlags(roomId);
}

void MainPresenter::setRoomTempSetPoint(Rooms roomId, float temperature)
{
    model->setRoomTempSetPoint(roomId, temperature);
}

void MainPresenter::setRoomFanSetPoint(Rooms roomId, HVAC_FanMode_t fanMode)
{
    model->setRoomFanSetPoint(roomId, fanMode);
}

void MainPresenter::setRoomUsbFlags(Rooms roomId, uint8_t usbFlags)
{
    model->setRoomUsbFlags(roomId, usbFlags);
}

bool MainPresenter::getIsFahrenheit()
{
    return model->getIsFahrenheit();
}

void MainPresenter::setIsFahrenheit(bool isFahrenheit)
{
    model->setIsFahrenheit(isFahrenheit);
}

Rooms MainPresenter::getSelectedRoom()
{
    return model->getSelectedRoom();;
}

void MainPresenter::setSelectedRoom(Rooms room)
{
    model->setSelectedRoom(room);
}

float MainPresenter::getRoomTempSetPoint(Rooms room)
{
    return model->getRoomTempSetPoint(room);
}

HVAC_FanMode_t MainPresenter::getRoomFanSetPoint(Rooms room)
{
    return model->getRoomFanSetPoint(room);
}

WeatherData MainPresenter::getWeatherData()
{
    return model->getWeatherData();
}

uint8_t MainPresenter::getClockHour()
{
    return model->getClockHour();
}

uint8_t MainPresenter::getClockMinute()
{
    return model->getClockMinute();
}

uint16_t MainPresenter::getClockYear()
{
    return model->getClockYear();
}

uint8_t MainPresenter::getClockMonth()
{
    return model->getClockMonth();
}

uint8_t MainPresenter::getClockDay()
{
    return model->getClockDay();
}

uint8_t MainPresenter::getClockWeekday()
{
    return model->getClockWeekday();
}

bool MainPresenter::getWiFiOnline()
{
    return model->getWiFiOnline();
}

bool MainPresenter::getRoomOnline(Rooms roomId)
{
    return model->getRoomOnline(roomId);
}
