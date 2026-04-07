#include <gui/containers/ScrollWheelContainer.hpp>
#include <touchgfx/Utils.hpp>
#include <cmath>

ScrollWheelContainer::ScrollWheelContainer() :
    scrollWheelAnimateToCallback(this, &ScrollWheelContainer::scrollWheelAnimateToHandler)
{

}

void ScrollWheelContainer::initialize()
{
    scrollWheel1.setAnimateToCallback(scrollWheelAnimateToCallback);

    Application::getInstance()->registerTimerWidget(this);
    ScrollWheelContainerBase::initialize();
}

void ScrollWheelContainer::handleTickEvent()
{
    emitIsAnimatingCallback(scrollWheel1.isAnimating());
}

void ScrollWheelContainer::scrollWheel1UpdateItem(MenuElement& item, int16_t itemIndex)
{
    item.setNumber(itemIndex, isFahrenheitSetting);
}

void ScrollWheelContainer::scrollWheel1UpdateCenterItem(MenuElementSelected& item, int16_t itemIndex)
{
    item.setNumber(itemIndex, isFahrenheitSetting);
}

// The callback updates the selectedIcon bast on the itemSelected parameter
void ScrollWheelContainer::scrollWheelAnimateToHandler(int16_t itemSelected)
{
    float tempValue;

    if (isFahrenheitSetting)
    {
        tempValue = (itemSelected + 18) / 1.8f;
    }
    else
    {
        tempValue = (itemSelected * 0.5f) + 10;
    }

    emitTempUpdatedCallback(tempValue);
}

void ScrollWheelContainer::setTempValue(float value, bool isFahrenheit)
{
    isFahrenheitSetting = isFahrenheit;
    initialize();

    int16_t index;

    if (isFahrenheitSetting)
    {
        index = int16_t(round(value * 1.8f - 18));
    }
    else
    {
        index = int16_t((value - 10) / 0.5f);
    }

    scrollWheel1.animateToItem(index);
}
