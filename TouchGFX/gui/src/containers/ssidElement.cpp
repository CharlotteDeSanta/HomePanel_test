#include <gui/containers/ssidElement.hpp>
#include <string.h>
#include <texts/TextKeysAndLanguages.hpp>
#include <touchgfx/Color.hpp>

ssidElement::ssidElement()
{
    memset(ssidTextBuffer, 0, sizeof(ssidTextBuffer));
    ssidTextArea.setPosition(0, 0, 360, 30);
    ssidTextArea.setColor(touchgfx::Color::getColorFromRGB(113, 132, 156));
    ssidTextArea.setLinespacing(0);
    ssidTextArea.setWildcard(ssidTextBuffer);
    ssidTextArea.setTypedText(touchgfx::TypedText(T_ENTEREDTEXT));
    ssidTextArea.resizeToCurrentText();
    ssidTextArea.setPosition(0, 0, 360, ssidTextArea.getHeight());
    add(ssidTextArea);
    ssidTextArea.centerY();
}

void ssidElement::initialize()
{
    ssidElementBase::initialize();
    ssidValueText.setVisible(false);
}

void ssidElement::setSSID(const char* ssid)
{
    touchgfx::Unicode::strncpy(ssidTextBuffer, (ssid != 0) ? ssid : "", MAX_SSID_TEXT_SIZE - 1);
    ssidTextBuffer[MAX_SSID_TEXT_SIZE - 1] = 0;
    ssidTextArea.resizeToCurrentText();
    ssidTextArea.centerX();
    ssidTextArea.centerY();
    ssidTextArea.invalidate();
}
