#include <gui/wifi_screen/WifiView.hpp>
#include <stdio.h>
#include <string.h>

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

bool splitManualJoinText(char* typedText, char** ssid, char** password)
{
    char* separator = 0;

    if ((typedText == 0) || (ssid == 0) || (password == 0))
    {
        return false;
    }

    separator = strchr(typedText, '|');
    if ((separator == 0) || (separator == typedText))
    {
        return false;
    }

    *separator = '\0';
    *ssid = typedText;
    *password = separator + 1;
    return true;
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
    passwordTextDirty(false)
{
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
    else if (&src == &confirmButton)
    {
#if defined(STM32H743xx)
        const char* selectedSsid = ssidScrollWheelContainer1.getSelectedSSID();
        const char* joinSsid = selectedSsid;
        const char* joinPassword = 0;
        char typedText[98] = {0};
        char* manualSsid = 0;
        char* manualPassword = 0;

        copyUnicodeToAscii(typedText,
                           static_cast<uint16_t>(sizeof(typedText)),
                           keyboard.getBuffer(),
                           keyboard.getBufferPosition());

        if (!isConnectableSsid(joinSsid))
        {
            if (!splitManualJoinText(typedText, &manualSsid, &manualPassword))
            {
                wifiUiLogConfirm(joinSsid, keyboard.getBufferPosition(), "blocked-invalid-ssid");
                return;
            }

            joinSsid = manualSsid;
            joinPassword = manualPassword;
            wifiUiLogConfirm(joinSsid,
                             static_cast<uint16_t>(strlen(joinPassword)),
                             (APP_WiFi_RequestJoin(joinSsid, joinPassword) != 0U) ? "manual-join-requested" : "manual-join-rejected");
            return;
        }

        joinPassword = typedText;
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

    for (uint16_t i = 0; i < MAX_PASSWORD_TEXT_SIZE; i++)
    {
        touchgfx::Unicode::UnicodeChar nextChar = (i < keyboardLength) ? keyboardBuffer[i] : 0;
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
