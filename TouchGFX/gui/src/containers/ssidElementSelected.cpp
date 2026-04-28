#include <gui/containers/ssidElementSelected.hpp>
#include <string.h>
#include <texts/TextKeysAndLanguages.hpp>
#include <touchgfx/Color.hpp>

ssidElementSelected::ssidElementSelected()
{
    memset(ssidTextBuffer, 0, sizeof(ssidTextBuffer));
    ssidTextArea.setPosition(0, 0, 360, 30);
    ssidTextArea.setColor(touchgfx::Color::getColorFromRGB(4, 72, 245));
    ssidTextArea.setLinespacing(0);
    ssidTextArea.setWildcard(ssidTextBuffer);
    ssidTextArea.setTypedText(touchgfx::TypedText(T_ENTEREDTEXT));
    ssidTextArea.resizeToCurrentText();
    ssidTextArea.setPosition(0, 0, 360, ssidTextArea.getHeight());
    add(ssidTextArea);
    ssidTextArea.centerY();
    ssidTextArea.setY(ssidTextArea.getY() + 7);
}

void ssidElementSelected::initialize()
{
    ssidElementSelectedBase::initialize();
    tempValueText.setVisible(false);
}

void ssidElementSelected::setSSID(const char* ssid)
{
    touchgfx::Unicode::strncpy(ssidTextBuffer, (ssid != 0) ? ssid : "", MAX_SSID_TEXT_SIZE - 1);
    ssidTextBuffer[MAX_SSID_TEXT_SIZE - 1] = 0;
    ssidTextArea.resizeToCurrentText();
    ssidTextArea.centerX();
    ssidTextArea.centerY();
    ssidTextArea.setY(ssidTextArea.getY() + 7);
    ssidTextArea.invalidate();
}
