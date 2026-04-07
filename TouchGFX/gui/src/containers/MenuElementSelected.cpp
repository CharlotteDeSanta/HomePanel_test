#include <gui/containers/MenuElementSelected.hpp>

MenuElementSelected::MenuElementSelected()
{

}

void MenuElementSelected::initialize()
{
    MenuElementSelectedBase::initialize();
}

void MenuElementSelected::setNumber(int index, bool isFahrenheit)
{
    float tempValue = (index * 0.5f) + 10;

    Unicode::snprintfFloat(tempValueTextBuffer, TEMPVALUETEXT_SIZE, "%#04.1f", tempValue);

    if (isFahrenheit)
    {
        tempValue = index + 50.0f;
        Unicode::snprintfFloat(tempValueTextBuffer, TEMPVALUETEXT_SIZE, "%.0f", tempValue);
    }
}
