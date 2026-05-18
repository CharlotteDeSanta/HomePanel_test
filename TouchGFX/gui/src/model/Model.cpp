#include <gui/model/Model.hpp>
#include <gui/model/ModelListener.hpp>
#include <gui/common/GuiTime.hpp>
#include <images/BitmapDatabase.hpp>
#include <texts/TextKeysAndLanguages.hpp>
#include "app_home_data.h"
#include "app_wifi_lwip.h"
#include <algorithm>

namespace
{
uint8_t parseTimeComponent(const char* digits)
{
    if (digits == 0 || digits[0] < '0' || digits[0] > '9' || digits[1] < '0' || digits[1] > '9')
    {
        return 0U;
    }

    return static_cast<uint8_t>((digits[0] - '0') * 10 + (digits[1] - '0'));
}

uint8_t parseBuildMonth()
{
    const char* month = __DATE__;
    switch (month[0])
    {
    case 'J':
        if (month[1] == 'a')
        {
            return 1U;
        }
        return (month[2] == 'n') ? 6U : 7U;
    case 'F':
        return 2U;
    case 'M':
        return (month[2] == 'r') ? 3U : 5U;
    case 'A':
        return (month[1] == 'p') ? 4U : 8U;
    case 'S':
        return 9U;
    case 'O':
        return 10U;
    case 'N':
        return 11U;
    case 'D':
        return 12U;
    default:
        return 1U;
    }
}

uint8_t parseBuildDay()
{
    const char* date = __DATE__;
    const uint8_t tens = (date[4] >= '0' && date[4] <= '9') ? static_cast<uint8_t>(date[4] - '0') : 0U;
    const uint8_t units = (date[5] >= '0' && date[5] <= '9') ? static_cast<uint8_t>(date[5] - '0') : 1U;
    return static_cast<uint8_t>(tens * 10U + units);
}

uint16_t parseBuildYear()
{
    const char* date = __DATE__;
    return static_cast<uint16_t>((date[7] - '0') * 1000 + (date[8] - '0') * 100 + (date[9] - '0') * 10 + (date[10] - '0'));
}

uint8_t calculateWeekday(uint16_t year, uint8_t month, uint8_t day)
{
    static const uint8_t monthOffsets[] = { 0U, 3U, 2U, 5U, 0U, 3U, 5U, 1U, 4U, 6U, 2U, 4U };
    year -= (month < 3U) ? 1U : 0U;
    return static_cast<uint8_t>((year + year / 4U - year / 100U + year / 400U + monthOffsets[month - 1U] + day) % 7U);
}

const uint32_t MODEL_CLOCK_UPDATE_PERIOD_MS = 1000U;
const uint32_t MODEL_CONTROLLER_POLL_PERIOD_MS = 1000U;
const uint32_t MODEL_GRAPH_SAMPLE_PERIOD_MS = 2000U;
const uint32_t MODEL_USB_FLAGS_CONFIRM_TIMEOUT_MS = 45000U;
const uint32_t MODEL_ROOM_ONLINE_TIMEOUT_MS = 45000U;
const float MODEL_DEFAULT_TARGET_TEMPERATURE_C = 25.0f;

bool tickReached(uint32_t nowMs, uint32_t deadlineMs)
{
    return static_cast<int32_t>(nowMs - deadlineMs) >= 0;
}

bool temperatureEquals(float lhs, float rhs)
{
    const float diff = lhs - rhs;
    return (diff > -0.05f) && (diff < 0.05f);
}
}

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
    wifiOnline(false),
    roomOnline{ false, false, false },
    unitIsFahrenheit(false),
    selectedRoom(KITCHEN),
    kitchenTempSetPoint(MODEL_DEFAULT_TARGET_TEMPERATURE_C),
    livingRoomTempSetPoint(MODEL_DEFAULT_TARGET_TEMPERATURE_C),
    bedRoomTempSetPoint(MODEL_DEFAULT_TARGET_TEMPERATURE_C),
    kitchenFanSetPoint(HVAC_FAN_AUTO),
    livingRoomFanSetPoint(HVAC_FAN_AUTO),
    bedRoomFanSetPoint(HVAC_FAN_AUTO),
    kitchenUsbFlags(0U),
    livingRoomUsbFlags(0U),
    bedRoomUsbFlags(0U),
    pendingUsbFlags{},
    usbFlagsPending{},
    usbFlagsPendingUntilMs{},
    clockYear(parseBuildYear()),
    clockMonth(parseBuildMonth()),
    clockDay(parseBuildDay()),
    clockWeekday(calculateWeekday(parseBuildYear(), parseBuildMonth(), parseBuildDay())),
    clockHour(parseTimeComponent(__TIME__)),
    clockMinute(parseTimeComponent(__TIME__ + 3)),
    lastClockUpdateMs(0U),
    lastIncomingDataPollMs(0U),
    lastGraphSampleMs(0U),
    runtimeTimingInitialized(false),
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

    GuiDateTime rtcDateTime = {};
    if (GUI_Time_GetDateTime(rtcDateTime))
    {
        clockYear = rtcDateTime.year;
        clockMonth = rtcDateTime.month;
        clockDay = rtcDateTime.day;
        clockWeekday = rtcDateTime.weekday;
        clockHour = rtcDateTime.hour;
        clockMinute = rtcDateTime.minute;
    }
}

void Model::tick()
{
    const uint32_t nowMs = GUI_Time_GetTickMs();

    if (!runtimeTimingInitialized)
    {
        lastClockUpdateMs = nowMs;
        lastIncomingDataPollMs = nowMs - MODEL_CONTROLLER_POLL_PERIOD_MS;
        lastGraphSampleMs = nowMs - MODEL_GRAPH_SAMPLE_PERIOD_MS;
        runtimeTimingInitialized = true;
        syncClockFromRtc(true);
        refreshConnectivityStatus();
    }

    while ((uint32_t)(nowMs - lastClockUpdateMs) >= MODEL_CLOCK_UPDATE_PERIOD_MS)
    {
        lastClockUpdateMs += MODEL_CLOCK_UPDATE_PERIOD_MS;
        syncClockFromRtc(false);
    }

    if ((uint32_t)(nowMs - lastIncomingDataPollMs) >= MODEL_CONTROLLER_POLL_PERIOD_MS)
    {
        lastIncomingDataPollMs += MODEL_CONTROLLER_POLL_PERIOD_MS;
        checkForIncomingData();
        refreshConnectivityStatus();
    }

    if ((uint32_t)(nowMs - lastGraphSampleMs) >= MODEL_GRAPH_SAMPLE_PERIOD_MS)
    {
        lastGraphSampleMs += MODEL_GRAPH_SAMPLE_PERIOD_MS;

        // Save data for kitchen
        bufferSample.temperature = kitchenTemperature;
        bufferSample.humidity = kitchenHumidity;
        bufferSample.fanMode = kitchenFanMode;
        insertBufferSample(kitchenBuffer, kitchenBufferCount, bufferSample);
        if (selectedRoom == KITCHEN && modelListener)
        {
            modelListener->sampleGraphData(bufferSample);
        }

        // Save data for living room
        bufferSample.temperature = livingRoomTemperature;
        bufferSample.humidity = livingRoomHumidity;
        bufferSample.fanMode = livingRoomFanMode;
        insertBufferSample(livingroomBuffer, livingroomBufferCount, bufferSample);
        if (selectedRoom == LIVINGROOM && modelListener)
        {
            modelListener->sampleGraphData(bufferSample);
        }

        // Save data for bedroom
        bufferSample.temperature = bedRoomTemperature;
        bufferSample.humidity = bedRoomHumidity;
        bufferSample.fanMode = bedRoomFanMode;
        insertBufferSample(bedroomBuffer, bedroomBufferCount, bufferSample);
        if (selectedRoom == BEDROOM && modelListener)
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

uint8_t Model::getRoomUsbFlags(Rooms roomId)
{
    switch (roomId)
    {
    case KITCHEN:
        return kitchenUsbFlags;
    case LIVINGROOM:
        return livingRoomUsbFlags;
    case BEDROOM:
        return bedRoomUsbFlags;
    default:
        return 0U;
    }
}

void Model::setRoomTempSetPoint(Rooms roomId, float temperature)
{
    HVAC_FanMode_t fanSetPoint = HVAC_FAN_UNKNOWN;
    uint8_t usbFlags = 0U;

    // Set model variable
    switch (roomId)
    {
    case KITCHEN:
        fanSetPoint = kitchenFanSetPoint;
        if (fanSetPoint != HVAC_FAN_AUTO)
        {
            return;
        }
        if (temperatureEquals(kitchenTempSetPoint, temperature))
        {
            return;
        }
        kitchenTempSetPoint = temperature;
        usbFlags = kitchenUsbFlags;
        break;
    case LIVINGROOM:
        fanSetPoint = livingRoomFanSetPoint;
        if (fanSetPoint != HVAC_FAN_AUTO)
        {
            return;
        }
        if (temperatureEquals(livingRoomTempSetPoint, temperature))
        {
            return;
        }
        livingRoomTempSetPoint = temperature;
        usbFlags = livingRoomUsbFlags;
        break;
    case BEDROOM:
        fanSetPoint = bedRoomFanSetPoint;
        if (fanSetPoint != HVAC_FAN_AUTO)
        {
            return;
        }
        if (temperatureEquals(bedRoomTempSetPoint, temperature))
        {
            return;
        }
        bedRoomTempSetPoint = temperature;
        usbFlags = bedRoomUsbFlags;
        break;
    default:
        return;
    }

    // Send event
    //outgoingEvent.timestamp = ;
    outgoingEvent = HVAC_Event_t{};
    outgoingEvent.roomId = (uint16_t)roomId;
    outgoingEvent.type = HVAC_EVENT_SETTINGS;
    outgoingEvent.room.settings.temperature = temperature;
    outgoingEvent.room.settings.mode = HVAC_MODE_UNKNOWN;
    outgoingEvent.room.settings.fan = fanSetPoint;
    outgoingEvent.room.settings.outputFlags = usbFlags;

    xHVAC_SendToController(&outgoingEvent, 10);
}

void Model::setRoomFanSetPoint(Rooms roomId, HVAC_FanMode_t fanMode)
{
    float temperature = 0.0f;
    uint8_t usbFlags = 0U;

    switch (roomId)
    {
    case KITCHEN:
        if (kitchenFanSetPoint == fanMode)
        {
            return;
        }
        kitchenFanSetPoint = fanMode;
        temperature = kitchenTempSetPoint;
        usbFlags = kitchenUsbFlags;
        break;
    case LIVINGROOM:
        if (livingRoomFanSetPoint == fanMode)
        {
            return;
        }
        livingRoomFanSetPoint = fanMode;
        temperature = livingRoomTempSetPoint;
        usbFlags = livingRoomUsbFlags;
        break;
    case BEDROOM:
        if (bedRoomFanSetPoint == fanMode)
        {
            return;
        }
        bedRoomFanSetPoint = fanMode;
        temperature = bedRoomTempSetPoint;
        usbFlags = bedRoomUsbFlags;
        break;
    default:
        return;
    }

    //outgoingEvent.timestamp = ;
    outgoingEvent = HVAC_Event_t{};
    outgoingEvent.roomId = (uint16_t)roomId;
    outgoingEvent.type = HVAC_EVENT_SETTINGS;
    outgoingEvent.room.settings.temperature = temperature;
    outgoingEvent.room.settings.mode = HVAC_MODE_UNKNOWN;
    outgoingEvent.room.settings.fan = fanMode;
    outgoingEvent.room.settings.outputFlags = usbFlags;

    xHVAC_SendToController(&outgoingEvent, 10);
}

void Model::setRoomUsbFlags(Rooms roomId, uint8_t usbFlags)
{
    float temperature = 0.0f;
    HVAC_FanMode_t fanSetPoint = HVAC_FAN_UNKNOWN;

    switch (roomId)
    {
    case KITCHEN:
        if (kitchenUsbFlags == usbFlags)
        {
            return;
        }
        kitchenUsbFlags = usbFlags;
        temperature = kitchenTempSetPoint;
        fanSetPoint = kitchenFanSetPoint;
        break;
    case LIVINGROOM:
        if (livingRoomUsbFlags == usbFlags)
        {
            return;
        }
        livingRoomUsbFlags = usbFlags;
        temperature = livingRoomTempSetPoint;
        fanSetPoint = livingRoomFanSetPoint;
        break;
    case BEDROOM:
        if (bedRoomUsbFlags == usbFlags)
        {
            return;
        }
        bedRoomUsbFlags = usbFlags;
        temperature = bedRoomTempSetPoint;
        fanSetPoint = bedRoomFanSetPoint;
        break;
    default:
        return;
    }

    outgoingEvent = HVAC_Event_t{};
    outgoingEvent.roomId = (uint16_t)roomId;
    outgoingEvent.type = HVAC_EVENT_SETTINGS;
    outgoingEvent.room.settings.temperature = temperature;
    outgoingEvent.room.settings.mode = HVAC_MODE_UNKNOWN;
    outgoingEvent.room.settings.fan = fanSetPoint;
    outgoingEvent.room.settings.outputFlags = usbFlags;

    if (xHVAC_SendToController(&outgoingEvent, 10))
    {
        markUsbFlagsPending(roomId, usbFlags);
    }
}

void Model::markUsbFlagsPending(Rooms roomId, uint8_t usbFlags)
{
    const uint8_t roomIndex = static_cast<uint8_t>(roomId);
    if (roomIndex >= 3U)
    {
        return;
    }

    pendingUsbFlags[roomIndex] = usbFlags;
    usbFlagsPending[roomIndex] = true;
    usbFlagsPendingUntilMs[roomIndex] = GUI_Time_GetTickMs() + MODEL_USB_FLAGS_CONFIRM_TIMEOUT_MS;
}

bool Model::shouldApplyUsbTelemetry(Rooms roomId, uint8_t usbFlags)
{
    const uint8_t roomIndex = static_cast<uint8_t>(roomId);
    if (roomIndex >= 3U)
    {
        return true;
    }

    if (!usbFlagsPending[roomIndex])
    {
        return true;
    }

    if (usbFlags == pendingUsbFlags[roomIndex])
    {
        usbFlagsPending[roomIndex] = false;
        return true;
    }

    if (!tickReached(GUI_Time_GetTickMs(), usbFlagsPendingUntilMs[roomIndex]))
    {
        return false;
    }

    usbFlagsPending[roomIndex] = false;
    return true;
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

uint16_t Model::getClockYear()
{
    return clockYear;
}

uint8_t Model::getClockMonth()
{
    return clockMonth;
}

uint8_t Model::getClockDay()
{
    return clockDay;
}

uint8_t Model::getClockWeekday()
{
    return clockWeekday;
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

bool Model::getWiFiOnline() const
{
    return wifiOnline;
}

bool Model::getRoomOnline(Rooms roomId) const
{
    const uint8_t roomIndex = static_cast<uint8_t>(roomId);
    if (roomIndex >= 3U)
    {
        return false;
    }

    return roomOnline[roomIndex];
}

void Model::refreshConnectivityStatus()
{
    const bool currentWiFiOnline = (APP_WiFi_LwIP_IsNetworkOnline() != 0U);

    if (currentWiFiOnline != wifiOnline)
    {
        wifiOnline = currentWiFiOnline;
        if (modelListener)
        {
            modelListener->updateWiFiOnline(wifiOnline);
        }
    }

    const uint32_t nowMs = GUI_Time_GetTickMs();
    for (uint8_t roomIndex = 0U; roomIndex < 3U; roomIndex++)
    {
        APP_HomeDataNodeStatus_t nodeStatus = {};
        bool currentRoomOnline = false;

        if (APP_HomeData_CopyNodeStatusByRoomIndex(roomIndex, &nodeStatus) != 0U)
        {
            const uint32_t ageMs = nowMs - nodeStatus.updated_ms;
            currentRoomOnline = (nodeStatus.online != 0U) && (ageMs <= MODEL_ROOM_ONLINE_TIMEOUT_MS);
        }

        const bool previousRoomOnline = roomOnline[roomIndex];
        if (previousRoomOnline != currentRoomOnline)
        {
            roomOnline[roomIndex] = currentRoomOnline;
            if (modelListener)
            {
                modelListener->updateRoomOnline(static_cast<Rooms>(roomIndex), currentRoomOnline);
            }

        }
    }
}

void Model::syncClockFromRtc(bool forceNotify)
{
    GuiDateTime rtcDateTime = {};
    if (!GUI_Time_GetDateTime(rtcDateTime))
    {
        return;
    }

    const bool dateChanged = forceNotify ||
                             (clockYear != rtcDateTime.year) ||
                             (clockMonth != rtcDateTime.month) ||
                             (clockDay != rtcDateTime.day) ||
                             (clockWeekday != rtcDateTime.weekday);
    const bool timeChanged = forceNotify ||
                             (clockHour != rtcDateTime.hour) ||
                             (clockMinute != rtcDateTime.minute);

    clockYear = rtcDateTime.year;
    clockMonth = rtcDateTime.month;
    clockDay = rtcDateTime.day;
    clockWeekday = rtcDateTime.weekday;
    clockHour = rtcDateTime.hour;
    clockMinute = rtcDateTime.minute;

    if (modelListener)
    {
        if (dateChanged)
        {
            modelListener->updateDate(clockYear, clockMonth, clockDay, clockWeekday);
        }

        if (timeChanged)
        {
            modelListener->updateClock(clockHour, clockMinute);
        }
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
                if (shouldApplyUsbTelemetry((Rooms)incomingEvent.roomId, incomingEvent.room.telemetry.outputFlags) &&
                    (incomingEvent.room.telemetry.outputFlags != kitchenUsbFlags))
                {
                    kitchenUsbFlags = incomingEvent.room.telemetry.outputFlags;
                    modelListener->updateUsbFlags((Rooms)incomingEvent.roomId, kitchenUsbFlags);
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
                if (shouldApplyUsbTelemetry((Rooms)incomingEvent.roomId, incomingEvent.room.telemetry.outputFlags) &&
                    (incomingEvent.room.telemetry.outputFlags != livingRoomUsbFlags))
                {
                    livingRoomUsbFlags = incomingEvent.room.telemetry.outputFlags;
                    modelListener->updateUsbFlags((Rooms)incomingEvent.roomId, livingRoomUsbFlags);
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
                if (shouldApplyUsbTelemetry((Rooms)incomingEvent.roomId, incomingEvent.room.telemetry.outputFlags) &&
                    (incomingEvent.room.telemetry.outputFlags != bedRoomUsbFlags))
                {
                    bedRoomUsbFlags = incomingEvent.room.telemetry.outputFlags;
                    modelListener->updateUsbFlags((Rooms)incomingEvent.roomId, bedRoomUsbFlags);
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
