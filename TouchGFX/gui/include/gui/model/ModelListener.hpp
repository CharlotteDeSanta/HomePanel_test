#ifndef MODELLISTENER_HPP
#define MODELLISTENER_HPP

#include <gui/model/Model.hpp>

class ModelListener
{
public:
    ModelListener() : model(0) {}

    virtual ~ModelListener() {}

    void bind(Model* m)
    {
        model = m;
    }

    virtual void updateTemperature(Rooms roomId, float temperature) {}
    virtual void updateHumidity(Rooms roomId, float humidity) {}
    virtual void updateFanMode(Rooms roomId, HVAC_FanMode_t fanMode) {}
    virtual void transmissionIsDone() {}
    virtual void updateClock(uint8_t clockHour, uint8_t clockMinute) {}
    virtual void updateTempSetPoint(Rooms roomId, float temperature) {}
    virtual void updateFanSetPoint(Rooms roomId, HVAC_FanMode_t fanSetPoint) {}
    virtual void updateWeatherData(WeatherData weatherData, bool isUnitFahrenheit) {}
    virtual void sampleGraphData(BufferSample graphSample) {}

protected:
    Model* model;
};

#endif // MODELLISTENER_HPP
