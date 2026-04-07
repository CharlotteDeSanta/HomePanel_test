#ifndef SCROLLWHEELCONTAINER_HPP
#define SCROLLWHEELCONTAINER_HPP

#include <gui_generated/containers/ScrollWheelContainerBase.hpp>

class ScrollWheelContainer : public ScrollWheelContainerBase
{
public:
    ScrollWheelContainer();
    virtual ~ScrollWheelContainer() {}

    virtual void initialize();
    virtual void handleTickEvent();
    virtual void scrollWheel1UpdateItem(MenuElement& item, int16_t itemIndex);
    virtual void scrollWheel1UpdateCenterItem(MenuElementSelected& item, int16_t itemIndex);
    virtual void setTempValue(float value, bool isFahrenheit);

private:
    // Callback which is executed when a item in the scroll whell is selected to as selected style.
    // The parameter itemSelected is the selected item.
    Callback<ScrollWheelContainer, int16_t> scrollWheelAnimateToCallback;
    void scrollWheelAnimateToHandler(int16_t itemSelected);
    bool isFahrenheitSetting = false;
};

#endif // SCROLLWHEELCONTAINER_HPP
