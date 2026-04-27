#include <gui/common/GuiTime.hpp>

#ifdef SIMULATOR

#include <chrono>
#include <ctime>

uint32_t GUI_Time_GetTickMs()
{
    using clock = std::chrono::steady_clock;
    static const clock::time_point start = clock::now();
    const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() - start).count();
    return static_cast<uint32_t>(elapsedMs);
}

bool GUI_Time_GetDateTime(GuiDateTime& dateTime)
{
    const std::time_t now = std::time(0);
    const std::tm* localTime = std::localtime(&now);

    if (localTime == 0)
    {
        return false;
    }

    dateTime.year = static_cast<uint16_t>(localTime->tm_year + 1900);
    dateTime.month = static_cast<uint8_t>(localTime->tm_mon + 1);
    dateTime.day = static_cast<uint8_t>(localTime->tm_mday);
    dateTime.weekday = static_cast<uint8_t>(localTime->tm_wday);
    dateTime.hour = static_cast<uint8_t>(localTime->tm_hour);
    dateTime.minute = static_cast<uint8_t>(localTime->tm_min);
    dateTime.second = static_cast<uint8_t>(localTime->tm_sec);
    return true;
}

#else

extern "C"
{
#include "rtc.h"
#include "stm32h7xx_hal.h"
}

uint32_t GUI_Time_GetTickMs()
{
    return HAL_GetTick();
}

bool GUI_Time_GetDateTime(GuiDateTime& dateTime)
{
    APP_RTC_DateTime_t rtcDateTime = {};

    if (APP_RTC_GetDateTime(&rtcDateTime) == 0U)
    {
        return false;
    }

    dateTime.year = rtcDateTime.year;
    dateTime.month = rtcDateTime.month;
    dateTime.day = rtcDateTime.day;
    dateTime.weekday = rtcDateTime.weekday;
    dateTime.hour = rtcDateTime.hour;
    dateTime.minute = rtcDateTime.minute;
    dateTime.second = rtcDateTime.second;
    return true;
}

#endif
