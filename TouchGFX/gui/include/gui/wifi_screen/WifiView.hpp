#ifndef WIFIVIEW_HPP
#define WIFIVIEW_HPP

#include <gui_generated/wifi_screen/WifiViewBase.hpp>
#include <gui/common/CustomKeyboard.hpp>
#include <gui/wifi_screen/WifiPresenter.hpp>
#include <touchgfx/Unicode.hpp>
#include <touchgfx/events/ClickEvent.hpp>

class WifiView : public WifiViewBase
{
public:
    static const uint16_t MAX_SELECTED_SSID_TEXT_SIZE = 34;
    static const uint16_t MAX_PASSWORD_TEXT_SIZE = 65;

    WifiView();
    virtual ~WifiView() {}
    virtual void setupScreen();
    virtual void tearDownScreen();
    virtual void handleClickEvent(const touchgfx::ClickEvent& event);
    virtual void handleTickEvent();
protected:
    enum InputFocus
    {
        FOCUS_SSID,
        FOCUS_PASSWORD
    };

    void actionButtonHandler(const touchgfx::AbstractButton& src);
    void keyboardBufferChangedHandler();
    void setInputFocus(InputFocus focus);
    void loadFocusedTextToKeyboard();
    void syncKeyboardBufferToFocusedText();
    void syncKeyboardBufferToSsidText();
    void syncKeyboardBufferToPasswordText();
    void syncSelectedSsidToTextArea();
    void clearTextBuffers();

    CustomKeyboard keyboard;
    touchgfx::Callback<WifiView, const touchgfx::AbstractButton&> actionButtonCallback;
    touchgfx::Callback<WifiView> keyboardBufferChangedCallback;
    InputFocus inputFocus;
    // Tracks the last SSID text mirrored from the scroll wheel so manual edits are not
    // overwritten unless the wheel selection actually changes.
    char lastSelectedSsidText[MAX_SELECTED_SSID_TEXT_SIZE];
    touchgfx::Unicode::UnicodeChar selectedSsidTextBuffer[MAX_SELECTED_SSID_TEXT_SIZE];
    touchgfx::Unicode::UnicodeChar passwordTextBuffer[MAX_PASSWORD_TEXT_SIZE];
};

#endif // WIFIVIEW_HPP
