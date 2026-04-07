#include "gui/common/hvac.hpp"
#include <stdlib.h>
#include <math.h>

#define NUMBER_OF_ROOMS 3

// Local functions
void updateTelemetry(HVAC_Event_t* event);

// Local variables
float tempSetPoint[NUMBER_OF_ROOMS] = { 22.0f, 23.5f, 21.5f };
HVAC_FanMode_t fanSetPoint[NUMBER_OF_ROOMS] = { HVAC_FAN_MED, HVAC_FAN_HIGH, HVAC_FAN_LOW };
float oldTemperature[NUMBER_OF_ROOMS] = { 22.0f, 23.3f, 21.5f };
float oldHumidity[NUMBER_OF_ROOMS] = { 55.1f, 42.7f, 46.4f };
bool firstEntrance = true;

typedef enum
{
    CLEAR_SKY = 0,
    MAINLY_CLEAR = 1,
    PARTLY_CLOUDY = 2,
    OVERCAST = 3,
    FOG = 45,
    RIME_FOG = 48,
    DRIZZLE_LIGHT = 51,
    DRIZZLE_MODERATE = 53,
    DRIZZLE_DENSE = 55,
    FREEZING_DRIZZLE_LIGHT = 56,
    FREEZING_DRIZZLE_DENSE = 57,
    RAIN_SLIGHT = 61,
    RAIN_MODERATE = 63,
    RAIN_HEAVY = 65,
    FREEZING_RAIN_LIGHT = 66,
    FREEZING_RAIN_HEAVY = 67,
    SNOW_FALL_SLIGHT = 71,
    SNOW_FALL_MODERATE = 73,
    SNOW_FALL_HEAVY = 75,
    SNOW_GRAINS = 77,
    RAIN_SHOWER_SLIGHT = 80,
    RAIN_SHOWER_MODERATE = 81,
    RAIN_SHOWER_VIOLENT = 82,
    SNOW_SHOWER_SLIGHT = 85,
    SNOW_SHOWER_HEAVY = 86,
    THUNDERSTORM = 95,
    THUNDERSTORM_HAIL_SLIGHT = 96,
    THUNDERSTORM_HAIL_HEAVY = 99
} WeatherCodes;

WeatherCodes validCodes[] = { CLEAR_SKY, MAINLY_CLEAR, PARTLY_CLOUDY, OVERCAST, FOG, RIME_FOG, DRIZZLE_LIGHT, DRIZZLE_MODERATE, DRIZZLE_DENSE, RAIN_SLIGHT, RAIN_MODERATE, RAIN_HEAVY,
                              RAIN_SHOWER_SLIGHT, RAIN_SHOWER_MODERATE, RAIN_SHOWER_VIOLENT, THUNDERSTORM, THUNDERSTORM_HAIL_SLIGHT, THUNDERSTORM_HAIL_HEAVY
                            };


bool xHVAC_SendToController(const HVAC_Event_t* event, uint32_t xTicksToWait)
{
    switch (event->roomId)
    {
    case 0:     // KITCHEN
        tempSetPoint[0] = event->room.settings.temperature;
        fanSetPoint[0] = event->room.settings.fan == HVAC_FAN_AUTO ? fanSetPoint[0] : event->room.settings.fan;
        break;
    case 1:     // LIVINGROOM
        tempSetPoint[1] = event->room.settings.temperature;
        fanSetPoint[1] = event->room.settings.fan == HVAC_FAN_AUTO ? fanSetPoint[1] : event->room.settings.fan;
        break;
    case 2:     // BEDROOM
        tempSetPoint[2] = event->room.settings.temperature;
        fanSetPoint[2] = event->room.settings.fan == HVAC_FAN_AUTO ? fanSetPoint[2] : event->room.settings.fan;
        break;
    default:
        break;
    }

    return 1;
}

bool xHVAC_ReceiveFromController(HVAC_Event_t* event, uint32_t xTicksToWait)
{
    // Send new room data
    if (!firstEntrance)
    {
        updateTelemetry(event);
    }

    // Set random weather data first time
    else
    {
        event->type = HVAC_EVENT_WEATHER;
        event->weather.current_temp = float(rand() % 10 + 15);
        uint8_t randomIndex = rand() % 18;
        event->weather.current_code = validCodes[randomIndex];
        event->weather.current_weekday = rand() % 7;

        // Generate random values for weather forecast
        for (uint8_t i = 0; i < 5; i++)
        {
            randomIndex = rand() % 18;
            event->weather.daily_code[i] = validCodes[randomIndex];
            event->weather.daily_temp_max[i] = float(rand() % 10 + 15);
            event->weather.daily_temp_min[i] = float(rand() % 10 + 5);
        }

        firstEntrance = false;
    }

    return 1;
}

// Simulate controller ready
bool xHVAC_WaitControllerReady(uint32_t xTicksToWait)
{
    return true;
}

void updateTelemetry(HVAC_Event_t* event)
{
    // Set random room and event type
    event->roomId = rand() % 3;
    event->type = HVAC_EVENT_TELEMETRY;

    // Calculate new simulated temperature
    float newTemperature = tempSetPoint[event->roomId] > oldTemperature[event->roomId] ? oldTemperature[event->roomId] + (rand() % 10) / 10.0f : oldTemperature[event->roomId] - (rand() % 10) / 10.0f;
    newTemperature = fminf(fmaxf(newTemperature, -10.0f), 40.0f); // Clamp between -10 and 40

    // Calculate new simulated humidity
    float newHumidity = oldHumidity[event->roomId] + (rand() % 50 - 25) / 10.0f;
    newHumidity = fminf(fmaxf(newHumidity, 0.0f), 100.0f); // Clamp between 0 and 100

    // Set telemetry data
    event->room.telemetry.fan = fanSetPoint[event->roomId];
    event->room.telemetry.humidity = newHumidity;
    event->room.telemetry.temperature = newTemperature;

    // Update old values
    oldHumidity[event->roomId] = newHumidity;
    oldTemperature[event->roomId] = newTemperature;
}
