#include <gui/containers/WeatherDaily.hpp>
#include <texts/TextKeysAndLanguages.hpp>

WeatherDaily::WeatherDaily()
{

}

void WeatherDaily::initialize()
{
    Application::getInstance()->registerTimerWidget(this);
    WeatherDailyBase::initialize();
}

void WeatherDaily::fadeInWithDelay(int delay)
{
    const uint8_t fadeTime = 20;

    //Set delays
    dayText.setFadeAnimationDelay(delay);
    weatherText.setFadeAnimationDelay(delay);
    minTemp.setFadeAnimationDelay(delay);
    maxTemp.setFadeAnimationDelay(fadeTime + delay);

    //Also setup up scling animation
    weatherLogo.setupAnimation(touchgfx::AnimationTextureMapper::SCALE, 1, 30, fadeTime + delay, EasingEquations::backEaseOut);

    //Start fade animation
    dayText.startFadeAnimation(255, fadeTime);
    weatherText.startFadeAnimation(255, fadeTime);
    maxTemp.startFadeAnimation(255, fadeTime);
    minTemp.startFadeAnimation(255, fadeTime);

    //Also start scaling animation of logo
    weatherLogo.startAnimation();
}

void WeatherDaily::setAllAlphas(int val)
{
    dayText.setAlpha(val);
    weatherText.setAlpha(val);
    maxTemp.setAlpha(val);
    minTemp.setAlpha(val);

    // Set logo scale to 0
    weatherLogo.setScale(0);
}

void WeatherDaily::setupDay(uint8_t dayIndex, WeatherData weatherData, bool isUnitFahrenheit)
{
    // Set name of day
    switch ((dayIndex + weatherData.weatherInfo.current_weekday) % 7)
    {
    case 0:     // Sunday
        dayText.setTypedText(T_SUNDAYTEXT);
        break;
    case 1:     // Monday
        dayText.setTypedText(T_MONDAYTEXT);
        break;
    case 2:     // Tuesday
        dayText.setTypedText(T_TUESDAYTEXT);
        break;
    case 3:     // Wednesday
        dayText.setTypedText(T_WEDNESDAYTEXT);
        break;
    case 4:     // Thursday
        dayText.setTypedText(T_THURSDAYTEXT);
        break;
    case 5:     // Friday
        dayText.setTypedText(T_FRIDAYTEXT);
        break;
    case 6:     // Saturday
        dayText.setTypedText(T_SATURDAYTEXT);
        break;
    default:
        dayText.setTypedText(T_MONDAYTEXT);
        break;
    }
    dayText.invalidate();

    // Set weather text
    weatherText.setTypedText(weatherData.dailyWeatherText[dayIndex]);
    weatherText.invalidate();

    // Set weather icon
    weatherLogo.setBitmap(weatherData.dailyWeatherBmp[dayIndex]);
    weatherLogo.setBitmapPosition(20, 20);
    weatherLogo.setWidth(136);
    weatherLogo.setHeight(136);
    weatherLogo.invalidate();

    // Set temperatures
    Unicode::snprintf(minTempBuffer, MINTEMP_SIZE, "%d",
                      (uint8_t)(isUnitFahrenheit ? weatherData.weatherInfo.daily_temp_min[dayIndex] * 1.8f + 32 : weatherData.weatherInfo.daily_temp_min[dayIndex]));
    Unicode::snprintf(maxTempBuffer, MAXTEMP_SIZE, "%d",
                      (uint8_t)(isUnitFahrenheit ? weatherData.weatherInfo.daily_temp_max[dayIndex] * 1.8f + 32 : weatherData.weatherInfo.daily_temp_max[dayIndex]));
    minTemp.invalidate();
    maxTemp.invalidate();
}
