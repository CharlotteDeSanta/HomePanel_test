#include <gui/wifi_screen/WifiView.hpp>

WifiView::WifiView()
{
    keyboard.setPosition(392, 220, 320, 240);
    add(keyboard);
}

void WifiView::setupScreen()
{
    WifiViewBase::setupScreen();
}

void WifiView::tearDownScreen()
{
    WifiViewBase::tearDownScreen();
}
