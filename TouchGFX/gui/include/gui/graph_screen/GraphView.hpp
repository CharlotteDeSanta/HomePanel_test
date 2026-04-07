#ifndef GRAPHVIEW_HPP
#define GRAPHVIEW_HPP

#include <gui_generated/graph_screen/GraphViewBase.hpp>
#include <gui/graph_screen/GraphPresenter.hpp>

class GraphView : public GraphViewBase
{
public:
    GraphView();
    virtual ~GraphView() {}

    virtual void setupScreen();
    virtual void tearDownScreen();
    virtual void handleTickEvent();
    virtual void temperatureButtonSelected();
    virtual void fanButtonSelected();
    virtual void humidityButtonSelected();
    virtual void roomChanged();
    virtual void fahrenheitToggleClicked();

    void updateClock(uint8_t hour, uint8_t minute);
    void insertGraphSample(BufferSample graphSample);

private:
    void setGraphData(BufferSample roomBuffer[], uint8_t roomBufferSize);

    const uint8_t FADE_DURATION = 40;
};

#endif // GRAPHVIEW_HPP
