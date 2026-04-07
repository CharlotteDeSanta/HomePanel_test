#include <gui/weather_screen/WeatherView.hpp>
#include <gui/weather_screen/WeatherPresenter.hpp>

WeatherPresenter::WeatherPresenter(WeatherView& v)
    : view(v)
{

}

void WeatherPresenter::activate()
{

}

void WeatherPresenter::deactivate()
{

}

void WeatherPresenter::updateWeatherData(WeatherData weatherData, bool isUnitFahrenheit)
{
    view.setWeatherData(weatherData, isUnitFahrenheit);
}

bool WeatherPresenter::getIsFahrenheit()
{
    return model->getIsFahrenheit();
}

void WeatherPresenter::setIsFahrenheit(bool isFahrenheit)
{
    model->setIsFahrenheit(isFahrenheit);
}

WeatherData WeatherPresenter::getWeatherData()
{
    return model->getWeatherData();
}
