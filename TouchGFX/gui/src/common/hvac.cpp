#include "gui/common/hvac.hpp"

#include "rtc.h"

#include <math.h>
#include <string.h>

namespace
{
struct SimRoomState
{
    const char* label;
    float currentTemperature;
    float currentHumidity;
    float targetTemperature;
    HVAC_Mode_t mode;
    HVAC_FanMode_t fanMode;
    float ambientTemperature;
    float ambientHumidity;
};

struct WeatherTemplate
{
    uint16_t code;
    float minTemperature;
    float maxTemperature;
};

constexpr uint8_t kRoomCount = HVAC_MAX_ROOM_COUNT;
constexpr uint32_t kWeatherRefreshPeriodMs = 5U * 60U * 1000U;

SimRoomState g_rooms[kRoomCount] = {
    { "Kitchen",    22.2f, 55.0f, 22.0f, HVAC_MODE_AUTO, HVAC_FAN_MED, 24.0f, 56.0f },
    { "LivingRoom", 23.4f, 46.0f, 23.5f, HVAC_MODE_AUTO, HVAC_FAN_HIGH, 25.0f, 47.0f },
    { "Bedroom",    21.6f, 49.0f, 21.5f, HVAC_MODE_AUTO, HVAC_FAN_LOW, 23.0f, 50.0f }
};

const WeatherTemplate g_weeklyWeather[7] = {
    { 0U,  16.0f, 24.0f }, // Sunday
    { 1U,  17.0f, 25.0f }, // Monday
    { 2U,  18.0f, 26.0f }, // Tuesday
    { 3U,  17.0f, 23.0f }, // Wednesday
    { 61U, 16.0f, 22.0f }, // Thursday
    { 80U, 15.0f, 21.0f }, // Friday
    { 95U, 14.0f, 20.0f }  // Saturday
};

HVAC_Weather_t g_weather = {};
bool g_controllerInitialized = false;
bool g_pendingWeatherEvent = true;
uint8_t g_nextTelemetryRoom = 0U;
uint32_t g_lastControllerUpdateMs = 0U;
uint32_t g_lastWeatherRefreshMs = 0U;

float clampFloat(float value, float minValue, float maxValue)
{
    return fminf(fmaxf(value, minValue), maxValue);
}

float roundToSingleDecimal(float value)
{
    return floorf(value * 10.0f + 0.5f) / 10.0f;
}

void copyLabel(char* destination, const char* source)
{
    uint8_t index = 0U;

    if ((destination == 0) || (source == 0))
    {
        return;
    }

    while ((index < HVAC_MAX_ROOM_LABEL_LENGTH) && (source[index] != '\0'))
    {
        destination[index] = source[index];
        index++;
    }

    destination[index] = '\0';
}

float getFanStrength(HVAC_FanMode_t fanMode, float difference)
{
    switch (fanMode)
    {
    case HVAC_FAN_OFF:
        return 0.15f;
    case HVAC_FAN_LOW:
        return 0.55f;
    case HVAC_FAN_MED:
        return 0.85f;
    case HVAC_FAN_HIGH:
        return 1.15f;
    case HVAC_FAN_AUTO:
    default:
        if (fabsf(difference) > HVAC_TEMP_HIGH_DIFF)
        {
            return 1.20f;
        }
        if (fabsf(difference) > HVAC_TEMP_MED_DIFF)
        {
            return 0.95f;
        }
        if (fabsf(difference) > HVAC_TEMP_LOW_DIFF)
        {
            return 0.70f;
        }
        return 0.40f;
    }
}

uint8_t getRtcDateTime(APP_RTC_DateTime_t& dateTime)
{
    return APP_RTC_GetDateTime(&dateTime);
}

uint16_t getCurrentWeatherCode(uint16_t baseCode, uint8_t hour)
{
    if ((hour < 6U) || (hour >= 21U))
    {
        return 0U;
    }

    if ((baseCode == 61U) || (baseCode == 80U) || (baseCode == 95U))
    {
        return baseCode;
    }

    if (hour < 10U)
    {
        return 1U;
    }

    if (hour < 16U)
    {
        return baseCode;
    }

    if (hour < 19U)
    {
        return 2U;
    }

    return 3U;
}

float getCurrentOutdoorTemperature(const WeatherTemplate& weather, uint8_t hour, uint8_t minute)
{
    const float currentHour = (float)hour + ((float)minute / 60.0f);

    if (currentHour < 6.0f)
    {
        return weather.minTemperature;
    }

    if (currentHour < 15.0f)
    {
        const float progress = (currentHour - 6.0f) / 9.0f;
        return weather.minTemperature + (weather.maxTemperature - weather.minTemperature) * progress;
    }

    if (currentHour < 24.0f)
    {
        const float progress = (currentHour - 15.0f) / 9.0f;
        return weather.maxTemperature - (weather.maxTemperature - weather.minTemperature) * progress;
    }

    return weather.minTemperature;
}

void refreshWeatherState(uint32_t nowMs)
{
    APP_RTC_DateTime_t dateTime = {};
    uint8_t weekday = 1U;
    uint8_t hour = 12U;
    uint8_t minute = 0U;

    if (getRtcDateTime(dateTime) != 0U)
    {
        weekday = dateTime.weekday % 7U;
        hour = dateTime.hour;
        minute = dateTime.minute;
    }
    else
    {
        const uint32_t seconds = nowMs / 1000U;
        weekday = (uint8_t)((seconds / 86400U) % 7U);
        hour = (uint8_t)((seconds / 3600U) % 24U);
        minute = (uint8_t)((seconds / 60U) % 60U);
    }

    const WeatherTemplate& today = g_weeklyWeather[weekday];

    g_weather.epoch_time = 0ULL;
    g_weather.current_weekday = weekday;
    g_weather.current_code = getCurrentWeatherCode(today.code, hour);
    g_weather.current_temp = roundToSingleDecimal(getCurrentOutdoorTemperature(today, hour, minute));

    for (uint8_t i = 0U; i < 5U; i++)
    {
        const WeatherTemplate& forecast = g_weeklyWeather[(weekday + i + 1U) % 7U];
        g_weather.daily_code[i] = forecast.code;
        g_weather.daily_temp_min[i] = forecast.minTemperature;
        g_weather.daily_temp_max[i] = forecast.maxTemperature;
    }
}

void stepRoomState(SimRoomState& room, uint32_t stepMs, uint8_t roomIndex, uint32_t nowMs)
{
    const float seconds = (float)stepMs / 1000.0f;
    const float delta = room.targetTemperature - room.currentTemperature;
    const float fanStrength = getFanStrength(room.fanMode, delta);
    const float ambientPull = (room.ambientTemperature - room.currentTemperature) * 0.06f * seconds;
    const float controlPull = delta * fanStrength * 0.18f * seconds;
    const uint32_t phase = ((nowMs / 1000U) + (uint32_t)roomIndex * 17U) % 48U;
    const float waveform = ((float)phase - 24.0f) * 0.0025f * seconds;

    room.currentTemperature = clampFloat(room.currentTemperature + ambientPull + controlPull + waveform, 16.0f, 32.0f);

    const float humidityTarget = room.ambientHumidity - ((room.currentTemperature - room.ambientTemperature) * 1.4f) - ((fanStrength - 0.4f) * 2.0f);
    const float humidityPull = (humidityTarget - room.currentHumidity) * 0.10f * seconds;
    room.currentHumidity = clampFloat(room.currentHumidity + humidityPull, 25.0f, 80.0f);

    room.currentTemperature = roundToSingleDecimal(room.currentTemperature);
    room.currentHumidity = roundToSingleDecimal(room.currentHumidity);
}

void stepController(uint32_t nowMs)
{
    if (!g_controllerInitialized)
    {
        return;
    }

    if (g_lastControllerUpdateMs == 0U)
    {
        g_lastControllerUpdateMs = nowMs;
    }

    const uint32_t stepMs = (nowMs > g_lastControllerUpdateMs) ? (nowMs - g_lastControllerUpdateMs) : 0U;
    const uint32_t effectiveStepMs = (stepMs == 0U) ? 1000U : stepMs;
    g_lastControllerUpdateMs = nowMs;

    for (uint8_t i = 0U; i < kRoomCount; i++)
    {
        stepRoomState(g_rooms[i], effectiveStepMs, i, nowMs);
    }

    if ((g_lastWeatherRefreshMs == 0U) || ((nowMs - g_lastWeatherRefreshMs) >= kWeatherRefreshPeriodMs))
    {
        refreshWeatherState(nowMs);
        g_lastWeatherRefreshMs = nowMs;
        g_pendingWeatherEvent = true;
    }
}

void fillWeatherEvent(HVAC_Event_t* event, uint32_t nowMs)
{
    *event = HVAC_Event_t{};
    event->timestamp = nowMs;
    event->type = HVAC_EVENT_WEATHER;
    event->weather = g_weather;
}

void fillTelemetryEvent(HVAC_Event_t* event, uint32_t nowMs)
{
    const uint8_t roomIndex = g_nextTelemetryRoom % kRoomCount;
    const SimRoomState& room = g_rooms[roomIndex];

    *event = HVAC_Event_t{};
    event->timestamp = nowMs;
    event->roomId = roomIndex;
    event->type = HVAC_EVENT_TELEMETRY;
    event->room.telemetry.timestamp = nowMs;
    event->room.telemetry.temperature = room.currentTemperature;
    event->room.telemetry.humidity = room.currentHumidity;
    event->room.telemetry.mode = room.mode;
    event->room.telemetry.fan = room.fanMode;

    g_nextTelemetryRoom = (uint8_t)((roomIndex + 1U) % kRoomCount);
}
} // namespace

bool xHVAC_ControllerInit()
{
    const uint32_t nowMs = HAL_GetTick();

    g_controllerInitialized = true;
    g_pendingWeatherEvent = true;
    g_nextTelemetryRoom = 0U;
    g_lastControllerUpdateMs = nowMs;
    g_lastWeatherRefreshMs = 0U;
    refreshWeatherState(nowMs);
    return true;
}

bool xHVAC_WaitControllerReady(uint32_t xTicksToWait)
{
    (void)xTicksToWait;

    if (!g_controllerInitialized)
    {
        xHVAC_ControllerInit();
    }

    return true;
}

bool xHVAC_SendToController(const HVAC_Event_t* event, uint32_t xTicksToWait)
{
    (void)xTicksToWait;

    if ((event == 0) || (event->roomId >= kRoomCount))
    {
        return false;
    }

    SimRoomState& room = g_rooms[event->roomId];

    if (event->type == HVAC_EVENT_SETTINGS)
    {
        room.targetTemperature = clampFloat(event->room.settings.temperature, 16.0f, 30.0f);
        room.mode = (event->room.settings.mode == HVAC_MODE_UNKNOWN) ? room.mode : event->room.settings.mode;
        room.fanMode = (event->room.settings.fan == HVAC_FAN_UNKNOWN) ? room.fanMode : event->room.settings.fan;
        return true;
    }

    return false;
}

bool xHVAC_ReceiveFromController(HVAC_Event_t* event, uint32_t xTicksToWait)
{
    (void)xTicksToWait;

    if (event == 0)
    {
        return false;
    }

    if (!g_controllerInitialized)
    {
        xHVAC_ControllerInit();
    }

    const uint32_t nowMs = HAL_GetTick();
    stepController(nowMs);

    if (g_pendingWeatherEvent)
    {
        fillWeatherEvent(event, nowMs);
        g_pendingWeatherEvent = false;
    }
    else
    {
        fillTelemetryEvent(event, nowMs);
    }

    return true;
}

bool xHVAC_GetRoomData(uint16_t idx, HVAC_Room_t* pxRoom, uint32_t xTicksToWait)
{
    (void)xTicksToWait;

    if ((pxRoom == 0) || (idx >= kRoomCount))
    {
        return false;
    }

    const SimRoomState& room = g_rooms[idx];
    *pxRoom = HVAC_Room_t{};
    pxRoom->telemetry.temperature = room.currentTemperature;
    pxRoom->telemetry.humidity = room.currentHumidity;
    pxRoom->telemetry.mode = room.mode;
    pxRoom->telemetry.fan = room.fanMode;
    pxRoom->settings.temperature = room.targetTemperature;
    pxRoom->settings.mode = room.mode;
    pxRoom->settings.fan = room.fanMode;
    copyLabel(pxRoom->metadata.label, room.label);
    return true;
}

bool xHVAC_SetRoomData(uint16_t idx, const HVAC_Room_t* pxRoom, uint32_t xTicksToWait)
{
    (void)xTicksToWait;

    if ((pxRoom == 0) || (idx >= kRoomCount))
    {
        return false;
    }

    SimRoomState& room = g_rooms[idx];
    room.targetTemperature = clampFloat(pxRoom->settings.temperature, 16.0f, 30.0f);
    room.mode = (pxRoom->settings.mode == HVAC_MODE_UNKNOWN) ? room.mode : pxRoom->settings.mode;
    room.fanMode = (pxRoom->settings.fan == HVAC_FAN_UNKNOWN) ? room.fanMode : pxRoom->settings.fan;
    return true;
}

bool xHVAC_GetWeather(HVAC_Weather_t* pxWeather)
{
    if (pxWeather == 0)
    {
        return false;
    }

    if (!g_controllerInitialized)
    {
        xHVAC_ControllerInit();
    }

    stepController(HAL_GetTick());
    *pxWeather = g_weather;
    return true;
}

const char* xHVAC_Mode2Str(HVAC_Mode_t mode)
{
    switch (mode)
    {
    case HVAC_MODE_OFF:
        return "off";
    case HVAC_MODE_COOL:
        return "cool";
    case HVAC_MODE_HEAT:
        return "heat";
    case HVAC_MODE_AUTO:
        return "auto";
    default:
        return "unknown";
    }
}

HVAC_Mode_t xHVAC_Str2Mode(const char* mode)
{
    if (mode == 0)
    {
        return HVAC_MODE_UNKNOWN;
    }

    if (strcmp(mode, "off") == 0)
    {
        return HVAC_MODE_OFF;
    }
    if (strcmp(mode, "cool") == 0)
    {
        return HVAC_MODE_COOL;
    }
    if (strcmp(mode, "heat") == 0)
    {
        return HVAC_MODE_HEAT;
    }
    if (strcmp(mode, "auto") == 0)
    {
        return HVAC_MODE_AUTO;
    }

    return HVAC_MODE_UNKNOWN;
}

const char* xHVAC_FanMode2Str(HVAC_FanMode_t mode)
{
    switch (mode)
    {
    case HVAC_FAN_OFF:
        return "off";
    case HVAC_FAN_LOW:
        return "low";
    case HVAC_FAN_MED:
        return "med";
    case HVAC_FAN_HIGH:
        return "high";
    case HVAC_FAN_AUTO:
        return "auto";
    default:
        return "unknown";
    }
}

HVAC_FanMode_t xHVAC_Str2FanMode(const char* mode)
{
    if (mode == 0)
    {
        return HVAC_FAN_UNKNOWN;
    }

    if (strcmp(mode, "off") == 0)
    {
        return HVAC_FAN_OFF;
    }
    if (strcmp(mode, "low") == 0)
    {
        return HVAC_FAN_LOW;
    }
    if ((strcmp(mode, "med") == 0) || (strcmp(mode, "medium") == 0))
    {
        return HVAC_FAN_MED;
    }
    if (strcmp(mode, "high") == 0)
    {
        return HVAC_FAN_HIGH;
    }
    if (strcmp(mode, "auto") == 0)
    {
        return HVAC_FAN_AUTO;
    }

    return HVAC_FAN_UNKNOWN;
}
