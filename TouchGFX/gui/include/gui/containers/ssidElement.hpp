#ifndef SSIDELEMENT_HPP
#define SSIDELEMENT_HPP

#include <gui_generated/containers/ssidElementBase.hpp>

class ssidElement : public ssidElementBase
{
public:
    ssidElement();
    virtual ~ssidElement() {}

    virtual void initialize();
protected:
};

#endif // SSIDELEMENT_HPP
