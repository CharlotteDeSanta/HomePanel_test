#ifndef WIFIVIEW_HPP
#define WIFIVIEW_HPP

#include <gui_generated/wifi_screen/WifiViewBase.hpp>
#include <gui/common/CustomKeyboard.hpp>
#include <gui/wifi_screen/WifiPresenter.hpp>
#include <touchgfx/Unicode.hpp>

class WifiView : public WifiViewBase
{
public:
    WifiView();
    virtual ~WifiView() {}
    virtual void setupScreen();
    virtual void tearDownScreen();
    virtual void handleTickEvent();
protected:
    void actionButtonHandler(const touchgfx::AbstractButton& src);
    void keyboardBufferChangedHandler();
    void syncKeyboardBufferToPasswordText();

    CustomKeyboard keyboard;
    touchgfx::Callback<WifiView, const touchgfx::AbstractButton&> actionButtonCallback;
    touchgfx::Callback<WifiView> keyboardBufferChangedCallback;
    bool passwordTextDirty;
};

#endif // WIFIVIEW_HPP
