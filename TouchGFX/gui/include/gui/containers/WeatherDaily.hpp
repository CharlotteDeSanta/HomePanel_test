#ifndef WEATHERDAILY_HPP
#define WEATHERDAILY_HPP

#include <gui_generated/containers/WeatherDailyBase.hpp>

class WeatherDaily : public WeatherDailyBase
{
public:
    WeatherDaily();
    virtual ~WeatherDaily() {}

    virtual void initialize();

    void fadeInWithDelay(int delay);
    void setAllAlphas(int val);
    void setupDay(uint8_t dayIndex, WeatherData weatherData, bool isUnitFahrenheit);
};

#endif // WEATHERDAILY_HPP
