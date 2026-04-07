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

void MainPresenter::updateClock(uint8_t clockHour, uint8_t clockMinute)
{
    view.updateClock(clockHour, clockMinute);
}

void MainPresenter::updateWeatherData(WeatherData weatherData, bool isUnitFahrenheit)
{
    view.updateCurrentWeather(weatherData, isUnitFahrenheit);
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

void MainPresenter::setRoomTempSetPoint(Rooms roomId, float temperature)
{
    model->setRoomTempSetPoint(roomId, temperature);
}

void MainPresenter::setRoomFanSetPoint(Rooms roomId, HVAC_FanMode_t fanMode)
{
    model->setRoomFanSetPoint(roomId, fanMode);
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
