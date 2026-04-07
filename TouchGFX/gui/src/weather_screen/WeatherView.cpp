#include <gui/weather_screen/WeatherView.hpp>
#include <texts/TextKeysAndLanguages.hpp>

WeatherView::WeatherView()
{

}

void WeatherView::setupScreen()
{
    WeatherViewBase::setupScreen();

    const uint8_t START_FADE_DELAY = 40;

    // Set temperature unit button
    if (presenter->getIsFahrenheit())
    {
        fahrenheitToggle.forceState(true);
    }

    // Set weather data
    setWeatherData(presenter->getWeatherData(), presenter->getIsFahrenheit());

    // Set alphas to 0
    weatherDay1.setAllAlphas(0);
    weatherDay2.setAllAlphas(0);
    weatherDay3.setAllAlphas(0);
    weatherDay4.setAllAlphas(0);
    weatherDay5.setAllAlphas(0);

    // Setup fade animations
    weatherDay1.fadeInWithDelay(0);
    weatherDay2.fadeInWithDelay(START_FADE_DELAY);
    weatherDay3.fadeInWithDelay(2 * START_FADE_DELAY);
    weatherDay4.fadeInWithDelay(3 * START_FADE_DELAY);
    weatherDay5.fadeInWithDelay(4 * START_FADE_DELAY);
}

void WeatherView::tearDownScreen()
{
    WeatherViewBase::tearDownScreen();
}

void WeatherView::fahrenheitToggleClicked()
{
    presenter->setIsFahrenheit(!presenter->getIsFahrenheit());

    setWeatherData(presenter->getWeatherData(), presenter->getIsFahrenheit());
}

void WeatherView::setWeatherData(WeatherData weatherData, bool isUnitFahrenheit)
{
    weatherDay1.setupDay(0, weatherData, isUnitFahrenheit);
    weatherDay2.setupDay(1, weatherData, isUnitFahrenheit);
    weatherDay3.setupDay(2, weatherData, isUnitFahrenheit);
    weatherDay4.setupDay(3, weatherData, isUnitFahrenheit);
    weatherDay5.setupDay(4, weatherData, isUnitFahrenheit);
}
