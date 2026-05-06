#include <gui/wifi_screen/WifiView.hpp>
#include <stdio.h>
#include <string.h>
#include <touchgfx/Drawable.hpp>

#if defined(STM32H743xx)
extern "C"
{
#include "app_wifi.h"
#include "app_debug_uart.h"
}
#endif

namespace
{
bool isConnectableSsid(const char* ssid)
{
    if ((ssid == 0) || (ssid[0] == '\0'))
    {
        return false;
    }

    if ((strcmp(ssid, "Scanning...") == 0) ||
        (strcmp(ssid, "No networks") == 0) ||
        (strcmp(ssid, "Scan aborted") == 0) ||
        (strcmp(ssid, "<hidden>") == 0))
    {
        return false;
    }

    return true;
}

void copyUnicodeToAscii(char* destination,
                        uint16_t destinationSize,
                        const touchgfx::Unicode::UnicodeChar* source,
                        uint16_t sourceLength)
{
    uint16_t index = 0U;

    if ((destination == 0) || (destinationSize == 0U))
    {
        return;
    }

    memset(destination, 0, destinationSize);
    if (source == 0)
    {
        return;
    }

    for (index = 0U; (index < sourceLength) && (index < (destinationSize - 1U)); index++)
    {
        const touchgfx::Unicode::UnicodeChar codepoint = source[index];
        destination[index] = (codepoint <= 0x7FU) ? static_cast<char>(codepoint) : '?';
    }
}

bool isPointInsideDrawable(const touchgfx::Drawable& drawable, int16_t x, int16_t y)
{
    const touchgfx::Rect rect = drawable.getAbsoluteRect();
    return rect.intersect(x, y);
}

#if defined(STM32H743xx)
void wifiUiLogConfirm(const char* ssid, uint16_t passwordLength, const char* result)
{
    char logBuffer[128] = {0};
    snprintf(logBuffer,
             sizeof(logBuffer),
             "[wifi-ui] confirm ssid=\"%s\" passLen=%u result=%s\n",
             (ssid != 0) ? ssid : "",
             static_cast<unsigned int>(passwordLength),
             (result != 0) ? result : "unknown");
    (void)APP_DebugUart_WriteString(logBuffer);
}
#endif
}

WifiView::WifiView() :
    actionButtonCallback(this, &WifiView::actionButtonHandler),
    keyboardBufferChangedCallback(this, &WifiView::keyboardBufferChangedHandler),
    inputFocus(FOCUS_SSID)
{
    memset(lastSelectedSsidText, 0, sizeof(lastSelectedSsidText));
    memset(selectedSsidTextBuffer, 0, sizeof(selectedSsidTextBuffer));
    memset(passwordTextBuffer, 0, sizeof(passwordTextBuffer));
    keyboard.setPosition(392, 220, 320, 240);
    keyboard.setBufferChangedCallback(keyboardBufferChangedCallback);
    add(keyboard);
}

void WifiView::setupScreen()
{
    WifiViewBase::setupScreen();
    clearButton.setAction(actionButtonCallback);
    confirmButton.setAction(actionButtonCallback);
    ssidScrollWheelContainer1.refreshScanResults();
    memset(selectedSsidTextBuffer, 0, sizeof(selectedSsidTextBuffer));
    ssidtextArea.setWildcard(selectedSsidTextBuffer);
    ssidtextArea.setPosition(400, 52, 386, 36);
    syncSelectedSsidToTextArea();
    memset(passwordTextBuffer, 0, sizeof(passwordTextBuffer));
    passwordtextArea.setWildcard(passwordTextBuffer);
    passwordtextArea.setPosition(400, 109, 386, 36);
    passwordtextArea.invalidate();
    setInputFocus(FOCUS_SSID);
}

void WifiView::tearDownScreen()
{
    WifiViewBase::tearDownScreen();
}

void WifiView::handleClickEvent(const touchgfx::ClickEvent& event)
{
    if (event.getType() == touchgfx::ClickEvent::PRESSED)
    {
        const int16_t x = event.getX();
        const int16_t y = event.getY();

        if (isPointInsideDrawable(boxWithBorder2, x, y))
        {
            setInputFocus(FOCUS_SSID);
        }
        else if (isPointInsideDrawable(boxWithBorder1, x, y))
        {
            setInputFocus(FOCUS_PASSWORD);
        }
    }

    WifiViewBase::handleClickEvent(event);
}

void WifiView::actionButtonHandler(const touchgfx::AbstractButton& src)
{
    if (&src == &clearButton)
    {
        clearTextBuffers();
        keyboard.clearBuffer();
    }
    else if (&src == &confirmButton)
    {
#if defined(STM32H743xx)
        char joinSsid[MAX_SELECTED_SSID_TEXT_SIZE] = {0};
        char joinPassword[MAX_PASSWORD_TEXT_SIZE] = {0};

        syncKeyboardBufferToFocusedText();
        copyUnicodeToAscii(joinSsid,
                           static_cast<uint16_t>(sizeof(joinSsid)),
                           selectedSsidTextBuffer,
                           MAX_SELECTED_SSID_TEXT_SIZE);
        copyUnicodeToAscii(joinPassword,
                           static_cast<uint16_t>(sizeof(joinPassword)),
                           passwordTextBuffer,
                           MAX_PASSWORD_TEXT_SIZE);

        if (!isConnectableSsid(joinSsid))
        {
            wifiUiLogConfirm(joinSsid, static_cast<uint16_t>(strlen(joinPassword)), "blocked-invalid-ssid");
            return;
        }

        wifiUiLogConfirm(joinSsid,
                         static_cast<uint16_t>(strlen(joinPassword)),
                         (APP_WiFi_RequestJoin(joinSsid, joinPassword) != 0U) ? "join-requested" : "join-rejected");
#endif
    }
}

void WifiView::handleTickEvent()
{
    ssidScrollWheelContainer1.refreshScanResults();
    syncSelectedSsidToTextArea();
    syncKeyboardBufferToFocusedText();
}

void WifiView::keyboardBufferChangedHandler()
{
    syncKeyboardBufferToFocusedText();
}

void WifiView::setInputFocus(InputFocus focus)
{
    if (inputFocus != focus)
    {
        syncKeyboardBufferToFocusedText();
        inputFocus = focus;
    }

    loadFocusedTextToKeyboard();
}

void WifiView::loadFocusedTextToKeyboard()
{
    if (inputFocus == FOCUS_SSID)
    {
        keyboard.setBufferText(selectedSsidTextBuffer, MAX_SELECTED_SSID_TEXT_SIZE);
    }
    else
    {
        keyboard.setBufferText(passwordTextBuffer, MAX_PASSWORD_TEXT_SIZE);
    }
}

void WifiView::syncKeyboardBufferToFocusedText()
{
    if (inputFocus == FOCUS_SSID)
    {
        syncKeyboardBufferToSsidText();
    }
    else
    {
        syncKeyboardBufferToPasswordText();
    }
}

void WifiView::syncKeyboardBufferToSsidText()
{
    const touchgfx::Unicode::UnicodeChar* keyboardBuffer = keyboard.getBuffer();
    const uint16_t keyboardLength = keyboard.getBufferPosition();
    const uint16_t textLimit = MAX_SELECTED_SSID_TEXT_SIZE - 1U;
    bool changed = false;

    for (uint16_t i = 0; i < MAX_SELECTED_SSID_TEXT_SIZE; i++)
    {
        touchgfx::Unicode::UnicodeChar nextChar = ((i < keyboardLength) && (i < textLimit)) ? keyboardBuffer[i] : 0;
        if (selectedSsidTextBuffer[i] != nextChar)
        {
            changed = true;
        }
        selectedSsidTextBuffer[i] = nextChar;
    }

    if (changed)
    {
        ssidtextArea.invalidate();
    }
}

void WifiView::syncKeyboardBufferToPasswordText()
{
    const touchgfx::Unicode::UnicodeChar* keyboardBuffer = keyboard.getBuffer();
    const uint16_t keyboardLength = keyboard.getBufferPosition();
    const uint16_t textLimit = MAX_PASSWORD_TEXT_SIZE - 1U;
    bool changed = false;

    for (uint16_t i = 0; i < MAX_PASSWORD_TEXT_SIZE; i++)
    {
        touchgfx::Unicode::UnicodeChar nextChar = ((i < keyboardLength) && (i < textLimit)) ? keyboardBuffer[i] : 0;
        if (passwordTextBuffer[i] != nextChar)
        {
            changed = true;
        }
        passwordTextBuffer[i] = nextChar;
    }

    if (changed)
    {
        passwordtextArea.invalidate();
    }
}

void WifiView::clearTextBuffers()
{
    const char* selectedSsid = ssidScrollWheelContainer1.getSelectedSSID();
    const char* safeSsid = isConnectableSsid(selectedSsid) ? selectedSsid : "";

    memset(selectedSsidTextBuffer, 0, sizeof(selectedSsidTextBuffer));
    memset(passwordTextBuffer, 0, sizeof(passwordTextBuffer));
    memset(lastSelectedSsidText, 0, sizeof(lastSelectedSsidText));
    strncpy(lastSelectedSsidText, safeSsid, sizeof(lastSelectedSsidText) - 1U);
    lastSelectedSsidText[sizeof(lastSelectedSsidText) - 1U] = '\0';
    ssidtextArea.invalidate();
    passwordtextArea.invalidate();
}

void WifiView::syncSelectedSsidToTextArea()
{
    const char* selectedSsid = ssidScrollWheelContainer1.getSelectedSSID();
    const char* safeSsid = isConnectableSsid(selectedSsid) ? selectedSsid : "";
    if (strcmp(safeSsid, lastSelectedSsidText) == 0)
    {
        return;
    }

    bool changed = false;

    memset(lastSelectedSsidText, 0, sizeof(lastSelectedSsidText));
    strncpy(lastSelectedSsidText, safeSsid, sizeof(lastSelectedSsidText) - 1U);
    lastSelectedSsidText[sizeof(lastSelectedSsidText) - 1U] = '\0';

    for (uint16_t i = 0; i < MAX_SELECTED_SSID_TEXT_SIZE; i++)
    {
        touchgfx::Unicode::UnicodeChar nextChar = 0;

        if ((i < (MAX_SELECTED_SSID_TEXT_SIZE - 1U)) && (safeSsid[i] != '\0'))
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

    if (inputFocus == FOCUS_SSID)
    {
        keyboard.setBufferText(selectedSsidTextBuffer, MAX_SELECTED_SSID_TEXT_SIZE);
    }
}
