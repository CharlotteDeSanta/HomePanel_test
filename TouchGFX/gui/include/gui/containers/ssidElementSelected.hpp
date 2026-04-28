#ifndef SSIDELEMENTSELECTED_HPP
#define SSIDELEMENTSELECTED_HPP

#include <gui_generated/containers/ssidElementSelectedBase.hpp>
#include <touchgfx/Unicode.hpp>
#include <touchgfx/widgets/TextAreaWithWildcard.hpp>

class ssidElementSelected : public ssidElementSelectedBase
{
public:
    static const uint16_t MAX_SSID_TEXT_SIZE = 34;

    ssidElementSelected();
    virtual ~ssidElementSelected() {}

    virtual void initialize();
    void setSSID(const char* ssid);
protected:
    touchgfx::TextAreaWithOneWildcard ssidTextArea;
    touchgfx::Unicode::UnicodeChar ssidTextBuffer[MAX_SSID_TEXT_SIZE];
};

#endif // SSIDELEMENTSELECTED_HPP
