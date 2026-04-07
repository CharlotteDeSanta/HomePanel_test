#include <gui/model/Model.hpp>
#include <gui/model/ModelListener.hpp>
#include <images/BitmapDatabase.hpp>
#include <texts/TextKeysAndLanguages.hpp>
#include <algorithm>

Model::Model() :
    kitchenTemperature(-20.0f),
    kitchenHumidity(-20.0f),
    kitchenFanMode(HVAC_FAN_UNKNOWN),
    livingRoomTemperature(-20.0f),
    livingRoomHumidity(-20.0f),
    livingRoomFanMode(HVAC_FAN_UNKNOWN),
    bedRoomTemperature(-20.0f),
    bedRoomHumidity(-20.0f),
    bedRoomFanMode(HVAC_FAN_UNKNOWN),
    unitIsFahrenheit(false),
    selectedRoom(KITCHEN),
    kitchenTempSetPoint(22),
    livingRoomTempSetPoint(23),
    bedRoomTempSetPoint(24),
    kitchenFanSetPoint(HVAC_FAN_AUTO),
    livingRoomFanSetPoint(HVAC_FAN_AUTO),
    bedRoomFanSetPoint(HVAC_FAN_AUTO),
    clockHour(9),
    clockMinute(0),
    weatherData{},
    bufferSample{},
    kitchenBufferCount(0),
    livingroomBufferCount(0),
    bedroomBufferCount(0),
    kitchenBuffer{},
    livingroomBuffer{},
    bedroomBuffer{},
    incomingEvent{},
    outgoingEvent{},
    modelListener(0)
{
    weatherData.currentWeatherBmp = getBitmapFromWeatherCode(0, true);
    for (uint8_t i = 0; i < 5; i++)
    {
        weatherData.dailyWeatherBmp[i] = getBitmapFromWeatherCode(incomingEvent.weather.daily_code[i], false);
        weatherData.dailyWeatherText[i] = getTextFromWeatherCode(incomingEvent.weather.daily_code[i]);
    }
}

void Model::tick()
{
    static int tickCount = 0;

    // Simulate data every second
    if (!(tickCount % 60))
    {
        checkForIncomingData();
    }

    tickCount++;

    // Increment clock
    if (!(tickCount % (60 * 60)))   // Increment minute variable
    {
        clockMinute++;

        if (clockMinute >= 60)
        {
            clockMinute = 0;
            clockHour++;

            if (clockHour >= 24)
            {
                clockHour = 0;
            }
        }

        modelListener->updateClock(clockHour, clockMinute);
    }

    // Sample graph data every other second
    if (!(tickCount % 120))
    {
        // Save data for kitchen
        bufferSample.temperature = kitchenTemperature;
        bufferSample.humidity = kitchenHumidity;
        bufferSample.fanMode = kitchenFanMode;
        insertBufferSample(kitchenBuffer, kitchenBufferCount, bufferSample);
        if (selectedRoom == KITCHEN)
        {
            modelListener->sampleGraphData(bufferSample);
        }

        // Save data for living room
        bufferSample.temperature = livingRoomTemperature;
        bufferSample.humidity = livingRoomHumidity;
        bufferSample.fanMode = livingRoomFanMode;
        insertBufferSample(livingroomBuffer, livingroomBufferCount, bufferSample);
        if (selectedRoom == LIVINGROOM)
        {
            modelListener->sampleGraphData(bufferSample);
        }

        // Save data for bedroom
        bufferSample.temperature = bedRoomTemperature;
        bufferSample.humidity = bedRoomHumidity;
        bufferSample.fanMode = bedRoomFanMode;
        insertBufferSample(bedroomBuffer, bedroomBufferCount, bufferSample);
        if (selectedRoom == BEDROOM)
        {
            modelListener->sampleGraphData(bufferSample);
        }
    }
}


float Model::getRoomTemperature(Rooms roomId)
{
    switch (roomId)
    {
    case KITCHEN:
        return kitchenTemperature;
        break;
    case LIVINGROOM:
        return livingRoomTemperature;
        break;
    case BEDROOM:
        return bedRoomTemperature;
        break;
    default:
        return 0;
    }
}

float Model::getRoomHumidity(Rooms roomId)
{
    switch (roomId)
    {
    case KITCHEN:
        return kitchenHumidity;
        break;
    case LIVINGROOM:
        return livingRoomHumidity;
        break;
    case BEDROOM:
        return bedRoomHumidity;
        break;
    default:
        return 0;
    }
}

HVAC_FanMode_t Model::getRoomFanMode(Rooms roomId)
{
    switch (roomId)
    {
    case KITCHEN:
        return kitchenFanMode;
        break;
    case LIVINGROOM:
        return livingRoomFanMode;
        break;
    case BEDROOM:
        return bedRoomFanMode;
        break;
    default:
        return HVAC_FAN_UNKNOWN;
    }
}

void Model::setRoomTempSetPoint(Rooms roomId, float temperature)
{
    // Set model variable
    switch (roomId)
    {
    case KITCHEN:
        kitchenTempSetPoint = temperature;
        break;
    case LIVINGROOM:
        livingRoomTempSetPoint = temperature;
        break;
    case BEDROOM:
        bedRoomTempSetPoint = temperature;
        break;
    default:
        break;
    }

    // Send event
    //outgoingEvent.timestamp = ;
    outgoingEvent.roomId = (uint16_t)roomId;
    outgoingEvent.type = HVAC_EVENT_SETTINGS;
    outgoingEvent.room.settings.temperature = temperature;

    // Use the current fan mode
    switch (roomId)
    {
    case KITCHEN:
        outgoingEvent.room.settings.fan = kitchenFanSetPoint;
        break;
    case LIVINGROOM:
        outgoingEvent.room.settings.fan = livingRoomFanSetPoint;
        break;
    case BEDROOM:
        outgoingEvent.room.settings.fan = bedRoomFanSetPoint;
        break;
    }

    xHVAC_SendToController(&outgoingEvent, 10);
}

void Model::setRoomFanSetPoint(Rooms roomId, HVAC_FanMode_t fanMode)
{
    //outgoingEvent.timestamp = ;
    outgoingEvent.roomId = (uint16_t)roomId;
    outgoingEvent.type = HVAC_EVENT_SETTINGS;
    outgoingEvent.room.settings.fan = fanMode;

    // Use the current temperature
    switch (roomId)
    {
    case KITCHEN:
        outgoingEvent.room.settings.temperature = kitchenTempSetPoint;
        break;
    case LIVINGROOM:
        outgoingEvent.room.settings.temperature = livingRoomTempSetPoint;
        break;
    case BEDROOM:
        outgoingEvent.room.settings.temperature = bedRoomTempSetPoint;
        break;
    }

    xHVAC_SendToController(&outgoingEvent, 10);

    switch (roomId)
    {
    case KITCHEN:
        kitchenFanSetPoint = fanMode;
        break;
    case LIVINGROOM:
        livingRoomFanSetPoint = fanMode;
        break;
    case BEDROOM:
        bedRoomFanSetPoint = fanMode;
        break;
    default:
        break;
    }
}

bool Model::getIsFahrenheit()
{
    return unitIsFahrenheit;
}

void Model::setIsFahrenheit(bool isFahrenheit)
{
    unitIsFahrenheit = isFahrenheit;
}

Rooms Model::getSelectedRoom()
{
    return selectedRoom;
}

void Model::setSelectedRoom(Rooms room)
{
    selectedRoom = room;
}

float Model::getRoomTempSetPoint(Rooms room)
{
    switch (room)
    {
    case KITCHEN:
        return kitchenTempSetPoint;
        break;
    case LIVINGROOM:
        return livingRoomTempSetPoint;
        break;
    case BEDROOM:
        return bedRoomTempSetPoint;
        break;
    default:
        return 0.0f;
    }
}

HVAC_FanMode_t Model::getRoomFanSetPoint(Rooms room)
{
    switch (room)
    {
    case KITCHEN:
        return kitchenFanSetPoint;
        break;
    case LIVINGROOM:
        return livingRoomFanSetPoint;
        break;
    case BEDROOM:
        return bedRoomFanSetPoint;
        break;
    default:
        return HVAC_FAN_UNKNOWN;
    }
}

WeatherData Model::getWeatherData()
{
    return weatherData;
}

uint8_t Model::getClockHour()
{
    return clockHour;
}

uint8_t Model::getClockMinute()
{
    return clockMinute;
}

void Model::getRoomBuffer(Rooms room, BufferSample returnBuffer[], uint8_t& returnBufferSize)
{
    returnBufferSize = 0;

    switch (room)
    {
    case KITCHEN:
        returnBufferSize = std::min((int)kitchenBufferCount, MAX_BUFFER_SIZE);
        std::copy(kitchenBuffer, kitchenBuffer + returnBufferSize, returnBuffer);
        break;
    case LIVINGROOM:
        returnBufferSize = std::min((int)livingroomBufferCount, MAX_BUFFER_SIZE);
        std::copy(livingroomBuffer, livingroomBuffer + returnBufferSize, returnBuffer);
        break;
    case BEDROOM:
        returnBufferSize = std::min((int)bedroomBufferCount, MAX_BUFFER_SIZE);
        std::copy(bedroomBuffer, bedroomBuffer + returnBufferSize, returnBuffer);
        break;
    }
}

void Model::checkForIncomingData()
{
    // Check if controller is ready
    if (xHVAC_WaitControllerReady(0) && xHVAC_ReceiveFromController(&incomingEvent, 0))
    {
        // Check for incoming event
        switch (incomingEvent.type)
        {
        // If incoming event is sensor data
        case HVAC_EVENT_TELEMETRY:
            switch ((Rooms)incomingEvent.roomId)
            {
            // Update screen if values have changed
            case KITCHEN:
                if (incomingEvent.room.telemetry.temperature != kitchenTemperature)
                {
                    modelListener->updateTemperature((Rooms)incomingEvent.roomId, incomingEvent.room.telemetry.temperature);
                    kitchenTemperature = incomingEvent.room.telemetry.temperature;
                }
                if (incomingEvent.room.telemetry.humidity != kitchenHumidity)
                {
                    modelListener->updateHumidity((Rooms)incomingEvent.roomId, incomingEvent.room.telemetry.humidity);
                    kitchenHumidity = incomingEvent.room.telemetry.humidity;
                }
                if (incomingEvent.room.telemetry.fan != kitchenFanMode)
                {
                    modelListener->updateFanMode((Rooms)incomingEvent.roomId, incomingEvent.room.telemetry.fan);
                    kitchenFanMode = incomingEvent.room.telemetry.fan;
                }
                break;
            case LIVINGROOM:
                if (incomingEvent.room.telemetry.temperature != livingRoomTemperature)
                {
                    modelListener->updateTemperature((Rooms)incomingEvent.roomId, incomingEvent.room.telemetry.temperature);
                    livingRoomTemperature = incomingEvent.room.telemetry.temperature;
                }
                if (incomingEvent.room.telemetry.humidity != livingRoomHumidity)
                {
                    modelListener->updateHumidity((Rooms)incomingEvent.roomId, incomingEvent.room.telemetry.humidity);
                    livingRoomHumidity = incomingEvent.room.telemetry.humidity;
                }
                if (incomingEvent.room.telemetry.fan != livingRoomFanMode)
                {
                    modelListener->updateFanMode((Rooms)incomingEvent.roomId, incomingEvent.room.telemetry.fan);
                    livingRoomFanMode = incomingEvent.room.telemetry.fan;
                }
                break;
            case BEDROOM:
                if (incomingEvent.room.telemetry.temperature != bedRoomTemperature)
                {
                    modelListener->updateTemperature((Rooms)incomingEvent.roomId, incomingEvent.room.telemetry.temperature);
                    bedRoomTemperature = incomingEvent.room.telemetry.temperature;
                }
                if (incomingEvent.room.telemetry.humidity != bedRoomHumidity)
                {
                    modelListener->updateHumidity((Rooms)incomingEvent.roomId, incomingEvent.room.telemetry.humidity);
                    bedRoomHumidity = incomingEvent.room.telemetry.humidity;
                }
                if (incomingEvent.room.telemetry.fan != bedRoomFanMode)
                {
                    modelListener->updateFanMode((Rooms)incomingEvent.roomId, incomingEvent.room.telemetry.fan);
                    bedRoomFanMode = incomingEvent.room.telemetry.fan;
                }
                break;
            }

            break;

        // If incoming event is settings
        case HVAC_EVENT_SETTINGS:
            switch ((Rooms)incomingEvent.roomId)
            {
            case KITCHEN:
                if (incomingEvent.room.settings.temperature != kitchenTempSetPoint)
                {
                    modelListener->updateTempSetPoint((Rooms)incomingEvent.roomId, incomingEvent.room.settings.temperature);
                    kitchenTempSetPoint = incomingEvent.room.settings.temperature;
                }
                if (incomingEvent.room.settings.fan != kitchenFanSetPoint)
                {
                    modelListener->updateFanSetPoint((Rooms)incomingEvent.roomId, incomingEvent.room.settings.fan);
                    kitchenFanSetPoint = incomingEvent.room.settings.fan;
                }
                break;
            case LIVINGROOM:
                if (incomingEvent.room.settings.temperature != livingRoomTempSetPoint)
                {
                    modelListener->updateTempSetPoint((Rooms)incomingEvent.roomId, incomingEvent.room.settings.temperature);
                    livingRoomTempSetPoint = incomingEvent.room.settings.temperature;
                }
                if (incomingEvent.room.settings.fan != livingRoomFanSetPoint)
                {
                    modelListener->updateFanSetPoint((Rooms)incomingEvent.roomId, incomingEvent.room.settings.fan);
                    livingRoomFanSetPoint = incomingEvent.room.settings.fan;
                }
                break;
            case BEDROOM:
                if (incomingEvent.room.settings.temperature != bedRoomTempSetPoint)
                {
                    modelListener->updateTempSetPoint((Rooms)incomingEvent.roomId, incomingEvent.room.settings.temperature);
                    bedRoomTempSetPoint = incomingEvent.room.settings.temperature;
                }
                if (incomingEvent.room.settings.fan != bedRoomFanSetPoint)
                {
                    modelListener->updateFanSetPoint((Rooms)incomingEvent.roomId, incomingEvent.room.settings.fan);
                    bedRoomFanSetPoint = incomingEvent.room.settings.fan;
                }
                break;
            }

            break;

        // If incoming event is time
        case HVAC_EVENT_TIME:

            break;

        // If incoming event is weather
        case HVAC_EVENT_WEATHER:
            weatherData.weatherInfo = incomingEvent.weather;
            weatherData.currentWeatherBmp = getBitmapFromWeatherCode(incomingEvent.weather.current_code, true);
            for (uint8_t i = 0; i < 5; i++)
            {
                weatherData.dailyWeatherBmp[i] = getBitmapFromWeatherCode(incomingEvent.weather.daily_code[i], false);
                weatherData.dailyWeatherText[i] = getTextFromWeatherCode(incomingEvent.weather.daily_code[i]);
            }

            modelListener->updateWeatherData(weatherData, unitIsFahrenheit);
            break;
        }
    }
}

uint16_t Model::getBitmapFromWeatherCode(uint16_t weatherCode, bool iconIsSmall)
{
    switch (weatherCode)
    {
    case CLEAR_SKY:
        return iconIsSmall ? BITMAP_W0_CLEAR_SKY_DAY_ID : BITMAP_W0_CLEAR_SKY_DAY_BIG_ID;
    case MAINLY_CLEAR:
        return iconIsSmall ? BITMAP_W1_MAINLY_CLEAR_ID : BITMAP_W1_MAINLY_CLEAR_BIG_ID;
    case PARTLY_CLOUDY:
        return iconIsSmall ? BITMAP_W2_PARTLY_CLOUDY_ID : BITMAP_W2_PARTLY_CLOUDY_BIG_ID;
    case OVERCAST:
        return iconIsSmall ? BITMAP_W3_OVERCAST_ID : BITMAP_W3_OVERCAST_BIG_ID;
    case FOG:
        return iconIsSmall ? BITMAP_W45_FOG_ID : BITMAP_W45_FOG_BIG_ID;
    case RIME_FOG:
        return iconIsSmall ? BITMAP_W48_DEPOSITING_RIME_FOG_ID : BITMAP_W48_DEPOSITING_RIME_FOG_BIG_ID;
    case DRIZZLE_LIGHT:
        return iconIsSmall ? BITMAP_W51_DRIZZLE_LIGHT_ID : BITMAP_W51_DRIZZLE_LIGHT_BIG_ID;
    case DRIZZLE_MODERATE:
        return iconIsSmall ? BITMAP_W53_DRIZZLE_MODERATE_ID : BITMAP_W53_DRIZZLE_MODERATE_BIG_ID;
    case DRIZZLE_DENSE:
        return iconIsSmall ? BITMAP_W55_DRIZZLE_DENSE_INTENSITY_ID : BITMAP_W55_DRIZZLE_DENSE_INTENSITY_BIG_ID;
    case FREEZING_DRIZZLE_LIGHT:
        return iconIsSmall ? BITMAP_W56_FREEZING_DRIZZLE_LIGHT_ID : BITMAP_W56_FREEZIND_RIZZLE_LIGHT_BIG_ID;
    case FREEZING_DRIZZLE_DENSE:
        return iconIsSmall ? BITMAP_W57_FREEZING_DRIZZLE_DENSE_INTENSITY_ID : BITMAP_W57_FREEZING_DRIZZLE_DENSE_INTENSITY_BIG_ID;
    case RAIN_SLIGHT:
        return iconIsSmall ? BITMAP_W61_RAIN_SLIGHT_ID : BITMAP_W61_RAIN_SLIGHT_BIG_ID;
    case RAIN_MODERATE:
        return iconIsSmall ? BITMAP_W63_RAIN_MODERATE_ID : BITMAP_W63_RAIN_MODERATE_BIG_ID;
    case RAIN_HEAVY:
        return iconIsSmall ? BITMAP_W65_RAIN_DENSE_INTENSITY_ID : BITMAP_W65_RAIN_DENSE_INTENSITY_BIG_ID;
    case FREEZING_RAIN_LIGHT:
        return iconIsSmall ? BITMAP_W66_FREEZING_RAIN_LIGHT_ID : BITMAP_W66_FREEZING_RAIN_LIGHT_BIG_ID;
    case FREEZING_RAIN_HEAVY:
        return iconIsSmall ? BITMAP_W67_FREEZING_RAIN_HEAVY_INTENSITY_ID : BITMAP_W67_FREEZING_RAIN_HEAVY_INTENSITY_BIG_ID;
    case SNOW_FALL_SLIGHT:
        return iconIsSmall ? BITMAP_W71_SNOW_FALL_SLIGHT_ID : BITMAP_W71_SNOW_FALL_SLIGHT_BIG_ID;
    case SNOW_FALL_MODERATE:
        return iconIsSmall ? BITMAP_W73_SNOW_FALL_MODERATE_ID : BITMAP_W73_SNOW_FALL_MODERATE_BIG_ID;
    case SNOW_FALL_HEAVY:
        return iconIsSmall ? BITMAP_W75_SNOW_FALL_HEAVY_INTENSITY_ID : BITMAP_W75_SNOW_FALL_HEAVY_INTENSITY_BIG_ID;
    case SNOW_GRAINS:
        return iconIsSmall ? BITMAP_W77_SNOW_GRAINS_ID : BITMAP_W77_SNOW_GRAINS_BIG_ID;
    case RAIN_SHOWER_SLIGHT:
        return iconIsSmall ? BITMAP_W80_RAIN_SHOWERS_SLIGHT_ID : BITMAP_W80_RAIN_SHOWERS_SLIGHT_BIG_ID;
    case RAIN_SHOWER_MODERATE:
        return iconIsSmall ? BITMAP_W81_RAIN_SHOWERS_MEDERATE_ID : BITMAP_W81_RAIN_SHOWERS_MEDERATE_BIG_ID;
    case RAIN_SHOWER_VIOLENT:
        return iconIsSmall ? BITMAP_W82_RAIN_SHOWERS_VIOLENT_ID : BITMAP_W82_RAIN_SHOWERS_VIOLENT_BIG_ID;
    case SNOW_SHOWER_SLIGHT:
        return iconIsSmall ? BITMAP_W85_SNOW_SHOWERS_SLIGHT_ID : BITMAP_W85_SNOW_SHOWERS_SLIGHT_BIG_ID;
    case SNOW_SHOWER_HEAVY:
        return iconIsSmall ? BITMAP_W86_SNOW_SHOWERS_HEAVY_ID : BITMAP_W86_SNOW_SHOWERS_HEAVY_BIG_ID;
    case THUNDERSTORM:
        return iconIsSmall ? BITMAP_W95_THUNDER_SLIGHT_ID : BITMAP_W95_THUNDER_SLIGHT_BIG_ID;
    case THUNDERSTORM_HAIL_SLIGHT:
        return iconIsSmall ? BITMAP_W96_THUNDERSTORM_SLIGHT_ID : BITMAP_W96_THUNSERSTORM_SLIGHT_BIG_ID;
    case THUNDERSTORM_HAIL_HEAVY:
        return iconIsSmall ? BITMAP_W99_THUNDERSTORM_HEAVY_HAIL_ID : BITMAP_W99_THUNSERSTORM_HEAVY_HAIL_BIG_ID;
    default:
        return iconIsSmall ? BITMAP_W0_CLEAR_SKY_DAY_ID : BITMAP_W0_CLEAR_SKY_DAY_BIG_ID;
    }
}

uint16_t Model::getTextFromWeatherCode(uint16_t weatherCode)
{
    switch (weatherCode)
    {
    case CLEAR_SKY:
        return T_CLEAR_SKYDAYTEXT;
    case MAINLY_CLEAR:
        return T_MAINLY_CLEARTEXT;
    case PARTLY_CLOUDY:
        return T_PARTLY_CLOUDYTEXT;
    case OVERCAST:
        return T_OVERCASTTEXT;
    case FOG:
        return T_FOGTEXT;
    case RIME_FOG:
        return T_RIME_FOGTEXT;
    case DRIZZLE_LIGHT:
        return T_DRIZZLE_LIGHTTEXT;
    case DRIZZLE_MODERATE:
        return T_DRIZZLE_MODERATETEXT;
    case DRIZZLE_DENSE:
        return T_DRIZZLE_DENSETEXT;
    case FREEZING_DRIZZLE_LIGHT:
        return T_FREEZING_DRIZZLE_LIGHTTEXT;
    case FREEZING_DRIZZLE_DENSE:
        return T_FREEZING_DRIZZLE_DENSETEXT;
    case RAIN_SLIGHT:
        return T_RAIN_SLIGHTTEXT;
    case RAIN_MODERATE:
        return T_RAIN_MODERATETEXT;
    case RAIN_HEAVY:
        return FREEZING_RAIN_HEAVY;
    case FREEZING_RAIN_LIGHT:
        return T_FREEZING_RAIN_LIGHTTEXT;
    case FREEZING_RAIN_HEAVY:
        return T_FREEZING_RAIN_HEAVYTEXT;
    case SNOW_FALL_SLIGHT:
        return T_SNOW_FALL_SLIGHTTEXT;
    case SNOW_FALL_MODERATE:
        return T_SNOW_FALL_MODERATETEXT;
    case SNOW_FALL_HEAVY:
        return T_SNOW_FALL_HEAVYTEXT;
    case SNOW_GRAINS:
        return T_SNOW_GRAINSTEXT;
    case RAIN_SHOWER_SLIGHT:
        return T_RAIN_SHOWER_SLIGHTTEXT;
    case RAIN_SHOWER_MODERATE:
        return T_RAIN_SHOWER_MODERATETEXT;
    case RAIN_SHOWER_VIOLENT:
        return T_RAIN_SHOWER_VIOLENTTEXT;
    case SNOW_SHOWER_SLIGHT:
        return T_SNOW_SHOWER_SLIGHTTEXT;
    case SNOW_SHOWER_HEAVY:
        return T_SNOW_SHOWER_HEAVYTEXT;
    case THUNDERSTORM:
        return T_THUNDERSTORMTEXT;
    case THUNDERSTORM_HAIL_SLIGHT:
        return T_THUNDERSTORM_HAIL_SLIGHTTEXT;
    case THUNDERSTORM_HAIL_HEAVY:
        return T_THUNDERSTORM_HAIL_HEAVYTEXT;
    default:
        return T_CLEAR_SKYDAYTEXT;
    }
}

void Model::insertBufferSample(BufferSample buffer[], uint8_t& bufferCount, const BufferSample& sample)
{
    if (bufferCount < MAX_BUFFER_SIZE)
    {
        // There is room in the buffer, insert the sample at the end
        buffer[bufferCount] = sample;
        bufferCount++;
    }
    else
    {
        // The buffer is full, shift elements to overwrite the oldest sample
        std::copy(buffer + 1, buffer + bufferCount, buffer);
        buffer[bufferCount - 1] = sample; // Overwrite the last sample
    }
}
