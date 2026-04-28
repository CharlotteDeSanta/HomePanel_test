#include <gui/wifi_screen/WifiView.hpp>
#include <string.h>

WifiView::WifiView() :
    actionButtonCallback(this, &WifiView::actionButtonHandler),
    keyboardBufferChangedCallback(this, &WifiView::keyboardBufferChangedHandler),
    passwordTextDirty(false)
{
    memset(selectedSsidTextBuffer, 0, sizeof(selectedSsidTextBuffer));
    keyboard.setPosition(392, 220, 320, 240);
    keyboard.setBufferChangedCallback(keyboardBufferChangedCallback);
    add(keyboard);
}

void WifiView::setupScreen()
{
    WifiViewBase::setupScreen();
    clearButton.setAction(actionButtonCallback);
    ssidScrollWheelContainer1.refreshScanResults();
    memset(selectedSsidTextBuffer, 0, sizeof(selectedSsidTextBuffer));
    ssidtextArea.setWildcard(selectedSsidTextBuffer);
    ssidtextArea.setPosition(400, 52, 386, 36);
    syncSelectedSsidToTextArea();
    memset(passwordtextAreaBuffer, 0, sizeof(passwordtextAreaBuffer));
    passwordtextArea.setWildcard(passwordtextAreaBuffer);
    passwordtextArea.setPosition(400, 109, 386, 36);
    passwordtextArea.invalidate();
}

void WifiView::tearDownScreen()
{
    WifiViewBase::tearDownScreen();
}

void WifiView::actionButtonHandler(const touchgfx::AbstractButton& src)
{
    if (&src == &clearButton)
    {
        keyboard.clearBuffer();
        syncKeyboardBufferToPasswordText();
    }
}

void WifiView::handleTickEvent()
{
    ssidScrollWheelContainer1.refreshScanResults();
    syncSelectedSsidToTextArea();
    syncKeyboardBufferToPasswordText();
}

void WifiView::keyboardBufferChangedHandler()
{
    passwordTextDirty = true;
}

void WifiView::syncKeyboardBufferToPasswordText()
{
    const touchgfx::Unicode::UnicodeChar* keyboardBuffer = keyboard.getBuffer();
    const uint16_t keyboardLength = keyboard.getBufferPosition();
    bool changed = false;

    for (uint16_t i = 0; i < PASSWORDTEXTAREA_SIZE; i++)
    {
        touchgfx::Unicode::UnicodeChar nextChar = (i < keyboardLength) ? keyboardBuffer[i] : 0;
        if (passwordtextAreaBuffer[i] != nextChar)
        {
            changed = true;
        }
        passwordtextAreaBuffer[i] = nextChar;
    }

    if (changed)
    {
        passwordtextArea.invalidate();
    }
}

void WifiView::syncSelectedSsidToTextArea()
{
    const char* selectedSsid = ssidScrollWheelContainer1.getSelectedSSID();
    const char* safeSsid = (selectedSsid != 0) ? selectedSsid : "";
    bool changed = false;

    for (uint16_t i = 0; i < MAX_SELECTED_SSID_TEXT_SIZE; i++)
    {
        touchgfx::Unicode::UnicodeChar nextChar = 0;

        if (safeSsid[i] != '\0')
        {
            nextChar = static_cast<touchgfx::Unicode::UnicodeChar>(safeSsid[i]);
        }

        if (selectedSsidTextBuffer[i] != nextChar)
        {
            changed = true;
            selectedSsidTextBuffer[i] = nextChar;
        }
    }

    if (changed)
    {
        ssidtextArea.invalidate();
    }
}
