#include <gui/containers/SpinningFanContainer.hpp>
#include <touchgfx/Utils.hpp>

SpinningFanContainer::SpinningFanContainer() :
    angularVelocity(0.0f),
    angularVelocityTarget(0.0f)
{

}

void SpinningFanContainer::initialize()
{
    Application::getInstance()->registerTimerWidget(this);
    SpinningFanContainerBase::initialize();
}

void SpinningFanContainer::handleTickEvent()
{
    if (abs(angularVelocityTarget - angularVelocity) < 0.001f) {} // Check if difference is less than 1/1000 and do nothing if so
    else if (angularVelocityTarget > angularVelocity)
    {
        angularVelocity += FULL_ANGULAR_VELOCITY / 100.0f;
    }
    else if (angularVelocityTarget < angularVelocity)
    {
        angularVelocity -= FULL_ANGULAR_VELOCITY / 100.0f;
    }

    if (angularVelocity > 0.001f)
    {
        fanTextureMapper.setZAngle(fanTextureMapper.getZAngle() + angularVelocity);
        fanTextureMapper.invalidateContent();
    }
}

void SpinningFanContainer::setFanSpeedPercentage(uint8_t speed)
{
    if (speed > 100)
    {
        speed = 100;
    }

    angularVelocityTarget = (FULL_ANGULAR_VELOCITY / 100.0f) * speed;
}

void SpinningFanContainer::setFanMode(HVAC_FanMode_t fanMode)
{
    if (!fanMode)
    {
        angularVelocityTarget = 0;
    }
    else if (fanMode <= HVAC_FAN_HIGH)
    {
        angularVelocityTarget = (FULL_ANGULAR_VELOCITY / 3.0f) * fanMode;
    }
    else
    {
        angularVelocityTarget = (FULL_ANGULAR_VELOCITY / 3.0f) * HVAC_FAN_MED;
    }
}
