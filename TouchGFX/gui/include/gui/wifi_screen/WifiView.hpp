#ifndef WIFIVIEW_HPP
#define WIFIVIEW_HPP

#include <gui_generated/wifi_screen/WifiViewBase.hpp>
#include <gui/common/CustomKeyboard.hpp>
#include <gui/wifi_screen/WifiPresenter.hpp>
#include <touchgfx/Unicode.hpp>

class WifiView : public WifiViewBase
{
public:
    static const uint16_t MAX_SELECTED_SSID_TEXT_SIZE = 34;
    static const uint16_t MAX_PASSWORD_TEXT_SIZE = 65;

    WifiView();
    virtual ~WifiView() {}
    virtual void setupScreen();
    virtual void tearDownScreen();
    virtual void handleTickEvent();
protected:
    void actionButtonHandler(const touchgfx::AbstractButton& src);
    void keyboardBufferChangedHandler();
    void syncKeyboardBufferToPasswordText();
    void syncSelectedSsidToTextArea();

    CustomKeyboard keyboard;
    touchgfx::Callback<WifiView, const touchgfx::AbstractButton&> actionButtonCallback;
    touchgfx::Callback<WifiView> keyboardBufferChangedCallback;
    bool passwordTextDirty;
    touchgfx::Unicode::UnicodeChar selectedSsidTextBuffer[MAX_SELECTED_SSID_TEXT_SIZE];
    touchgfx::Unicode::UnicodeChar passwordTextBuffer[MAX_PASSWORD_TEXT_SIZE];
};

#endif // WIFIVIEW_HPP
