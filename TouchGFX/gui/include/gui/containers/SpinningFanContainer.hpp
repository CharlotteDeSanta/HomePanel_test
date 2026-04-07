#ifndef SPINNINGFANCONTAINER_HPP
#define SPINNINGFANCONTAINER_HPP

#include <gui_generated/containers/SpinningFanContainerBase.hpp>

class SpinningFanContainer : public SpinningFanContainerBase
{
public:
    SpinningFanContainer();
    virtual ~SpinningFanContainer() {}

    virtual void initialize();
    virtual void handleTickEvent();

    void setFanSpeedPercentage(uint8_t speed);
    void setFanMode(HVAC_FanMode_t fanMode);

private:
    const float FULL_ANGULAR_VELOCITY = 0.2f;
    float angularVelocity;
    float angularVelocityTarget;
};

#endif // SPINNINGFANCONTAINER_HPP
