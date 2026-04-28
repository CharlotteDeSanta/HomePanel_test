#include <gui/wifi_screen/WifiView.hpp>
#include <string.h>

WifiView::WifiView() :
    actionButtonCallback(this, &WifiView::actionButtonHandler),
    keyboardBufferChangedCallback(this, &WifiView::keyboardBufferChangedHandler),
    passwordTextDirty(false)
{
    keyboard.setPosition(392, 220, 320, 240);
    keyboard.setBufferChangedCallback(keyboardBufferChangedCallback);
    add(keyboard);
}

void WifiView::setupScreen()
{
    WifiViewBase::setupScreen();
    clearButton.setAction(actionButtonCallback);
    memset(passwordtextAreaBuffer, 0, sizeof(passwordtextAreaBuffer));
    passwordtextArea.setWildcard(passwordtextAreaBuffer);
    passwordtextArea.setPosition(400, 82, 386, 36);
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
