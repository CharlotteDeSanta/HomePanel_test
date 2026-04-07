#ifndef ROOMCARD_HPP
#define ROOMCARD_HPP

#include <gui_generated/containers/RoomCardBase.hpp>
#include <texts/TextKeysAndLanguages.hpp>

class RoomCard : public RoomCardBase
{
public:
    RoomCard();
    virtual ~RoomCard() {}

    virtual void initialize();
    virtual void handleTickEvent();
    virtual void startupAnimation(uint16_t duration, uint16_t sequenceNumber, TypedText roomName);
    virtual void flipOut(uint16_t duration);

    void setTemperature(float temp, bool isUnitFahrenheit);
    void setFanSetPoint(HVAC_FanMode_t fanMode);
    void setHumidity(float hum);

private:
    bool initiated = 0;
    bool deinitiated = 0;
    uint16_t initDelay = 0;
    float rotationSpeed = 0;
    uint8_t animationDuration;
};

#endif // ROOMCARD_HPP
