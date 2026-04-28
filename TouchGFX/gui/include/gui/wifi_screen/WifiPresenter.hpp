#ifndef WIFIPRESENTER_HPP
#define WIFIPRESENTER_HPP

#include <gui/model/ModelListener.hpp>
#include <mvp/Presenter.hpp>

using namespace touchgfx;

class WifiView;

class WifiPresenter : public touchgfx::Presenter, public ModelListener
{
public:
    WifiPresenter(WifiView& v);

    /**
     * The activate function is called automatically when this screen is "switched in"
     * (ie. made active). Initialization logic can be placed here.
     */
    virtual void activate();

    /**
     * The deactivate function is called automatically when this screen is "switched out"
     * (ie. made inactive). Teardown functionality can be placed here.
     */
    virtual void deactivate();

    virtual ~WifiPresenter() {}

private:
    WifiPresenter();

    WifiView& view;
};

#endif // WIFIPRESENTER_HPP
