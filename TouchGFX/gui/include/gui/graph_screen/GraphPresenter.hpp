#ifndef GRAPHPRESENTER_HPP
#define GRAPHPRESENTER_HPP

#include <gui/model/ModelListener.hpp>
#include <mvp/Presenter.hpp>

using namespace touchgfx;

class GraphView;

class GraphPresenter : public touchgfx::Presenter, public ModelListener
{
public:
    GraphPresenter(GraphView& v);

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

    virtual ~GraphPresenter() {}

    virtual void updateClock(uint8_t clockHour, uint8_t clockMinute);
    virtual void sampleGraphData(BufferSample graphSample);

    float getRoomTemperature(Rooms roomId);
    float getRoomHumidity(Rooms roomId);
    HVAC_FanMode_t getRoomFanMode(Rooms roomId);
    bool getIsFahrenheit();
    void setIsFahrenheit(bool isFahrenheit);
    Rooms getSelectedRoom();
    uint8_t getClockHour();
    uint8_t getClockMinute();
    void getRoomBuffer(Rooms room, BufferSample returnBuffer[], uint8_t& returnBufferSize);
    void setSelectedRoom(Rooms room);

private:
    GraphPresenter();

    GraphView& view;
};

#endif // GRAPHPRESENTER_HPP
