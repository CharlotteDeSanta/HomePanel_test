#ifndef MENUELEMENTSELECTED_HPP
#define MENUELEMENTSELECTED_HPP

#include <gui_generated/containers/MenuElementSelectedBase.hpp>

class MenuElementSelected : public MenuElementSelectedBase
{
public:
    MenuElementSelected();
    virtual ~MenuElementSelected() {}

    virtual void initialize();

    void setNumber(int index, bool isFahrenheit);
};

#endif // MENUELEMENTSELECTED_HPP
