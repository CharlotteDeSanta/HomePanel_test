#ifndef WIFIVIEW_HPP
#define WIFIVIEW_HPP

#include <gui_generated/wifi_screen/WifiViewBase.hpp>
#include <gui/common/CustomKeyboard.hpp>
#include <gui/wifi_screen/WifiPresenter.hpp>

class WifiView : public WifiViewBase
{
public:
    WifiView();
    virtual ~WifiView() {}
    virtual void setupScreen();
    virtual void tearDownScreen();
protected:
    CustomKeyboard keyboard;
};

#endif // WIFIVIEW_HPP
