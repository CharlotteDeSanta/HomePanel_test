#ifndef SSIDELEMENT_HPP
#define SSIDELEMENT_HPP

#include <gui_generated/containers/ssidElementBase.hpp>
#include <touchgfx/Unicode.hpp>
#include <touchgfx/widgets/TextAreaWithWildcard.hpp>

class ssidElement : public ssidElementBase
{
public:
    static const uint16_t MAX_SSID_TEXT_SIZE = 34;

    ssidElement();
    virtual ~ssidElement() {}

    virtual void initialize();
    void setSSID(const char* ssid);
protected:
    touchgfx::TextAreaWithOneWildcard ssidTextArea;
    touchgfx::Unicode::UnicodeChar ssidTextBuffer[MAX_SSID_TEXT_SIZE];
};

#endif // SSIDELEMENT_HPP
