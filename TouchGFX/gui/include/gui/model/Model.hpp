#ifndef MODEL_HPP
#define MODEL_HPP

#include <stdint.h>
#include "gui/common/hvac.hpp"

#define MAX_BUFFER_SIZE 31

class ModelListener;

typedef enum
{
    KITCHEN,
    LIVINGROOM,
    BEDROOM
} Rooms;

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

typedef struct
{
    HVAC_Weather_t  weatherInfo;
    uint16_t        currentWeatherBmp;
    uint16_t        dailyWeatherBmp[5];
    uint16_t        dailyWeatherText[5];
} WeatherData;

typedef struct
{
    float           temperature;
    float           humidity;
    HVAC_FanMode_t  fanMode;
} BufferSample;

class Model
{
public:
    Model();

    void bind(ModelListener* listener)
    {
        modelListener = listener;
    }

    void tick();
    float getRoomTemperature(Rooms roomId);
    float getRoomHumidity(Rooms roomId);
    HVAC_FanMode_t getRoomFanMode(Rooms roomId);
    uint8_t getRoomUsbFlags(Rooms roomId);
    void setRoomTempSetPoint(Rooms roomId, float temperature);
    void setRoomFanSetPoint(Rooms roomId, HVAC_FanMode_t fanMode);
    void setRoomUsbFlags(Rooms roomId, uint8_t usbFlags);
    bool getIsFahrenheit();
    void setIsFahrenheit(bool isFahrenheit);
    Rooms getSelectedRoom();
    void setSelectedRoom(Rooms room);
    float getRoomTempSetPoint(Rooms room);
    HVAC_FanMode_t getRoomFanSetPoint(Rooms room);
    WeatherData getWeatherData();
    uint8_t getClockHour();
    uint8_t getClockMinute();
    uint16_t getClockYear();
    uint8_t getClockMonth();
    uint8_t getClockDay();
    uint8_t getClockWeekday();
    void getRoomBuffer(Rooms room, BufferSample returnBuffer[], uint8_t& returnBufferSize);
    bool getWiFiOnline() const;
    bool getRoomOnline(Rooms roomId) const;

private:
    void applyRoomDefaultAutoSetPoint(Rooms roomId);
    void checkForIncomingData();
    void refreshConnectivityStatus();
    void markUsbFlagsPending(Rooms roomId, uint8_t usbFlags);
    bool shouldApplyUsbTelemetry(Rooms roomId, uint8_t usbFlags);
    uint16_t getBitmapFromWeatherCode(uint16_t weatherCode, bool iconIsSmall);
    uint16_t getTextFromWeatherCode(uint16_t weatherCode);
    void insertBufferSample(BufferSample buffer[], uint8_t& bufferCount, const BufferSample& sample);
    void syncClockFromRtc(bool forceNotify);

    float kitchenTemperature;
    float kitchenHumidity;
    HVAC_FanMode_t kitchenFanMode;
    float livingRoomTemperature;
    float livingRoomHumidity;
    HVAC_FanMode_t livingRoomFanMode;
    float bedRoomTemperature;
    float bedRoomHumidity;
    HVAC_FanMode_t bedRoomFanMode;
    bool wifiOnline;
    bool roomOnline[3];
    bool unitIsFahrenheit;
    Rooms selectedRoom;
    float kitchenTempSetPoint;
    float livingRoomTempSetPoint;
    float bedRoomTempSetPoint;
    HVAC_FanMode_t kitchenFanSetPoint;
    HVAC_FanMode_t livingRoomFanSetPoint;
    HVAC_FanMode_t bedRoomFanSetPoint;
    uint8_t kitchenUsbFlags;
    uint8_t livingRoomUsbFlags;
    uint8_t bedRoomUsbFlags;
    uint8_t pendingUsbFlags[3];
    bool usbFlagsPending[3];
    uint32_t usbFlagsPendingUntilMs[3];
    uint16_t clockYear;
    uint8_t clockMonth;
    uint8_t clockDay;
    uint8_t clockWeekday;
    uint8_t clockHour;
    uint8_t clockMinute;
    uint32_t lastClockUpdateMs;
    uint32_t lastIncomingDataPollMs;
    uint32_t lastGraphSampleMs;
    bool runtimeTimingInitialized;
    WeatherData weatherData;
    BufferSample bufferSample;

    // Counters for the number of samples in each buffer
    uint8_t kitchenBufferCount;
    uint8_t livingroomBufferCount;
    uint8_t bedroomBufferCount;

    // Buffers for graph data
    BufferSample kitchenBuffer[MAX_BUFFER_SIZE];
    BufferSample livingroomBuffer[MAX_BUFFER_SIZE];
    BufferSample bedroomBuffer[MAX_BUFFER_SIZE];

    HVAC_Event_t incomingEvent;     // Object to handle events from queue
    HVAC_Event_t outgoingEvent;     // Object to send events to queue

    ModelListener* modelListener;
};

#endif // MODEL_HPP
