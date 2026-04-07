#include <gui/graph_screen/GraphView.hpp>
#include <gui/graph_screen/GraphPresenter.hpp>

GraphPresenter::GraphPresenter(GraphView& v)
    : view(v)
{

}

void GraphPresenter::activate()
{

}

void GraphPresenter::deactivate()
{

}

void GraphPresenter::updateClock(uint8_t clockHour, uint8_t clockMinute)
{
    view.updateClock(clockHour, clockMinute);
}

void GraphPresenter::sampleGraphData(BufferSample graphSample)
{
    view.insertGraphSample(graphSample);
}

float GraphPresenter::getRoomTemperature(Rooms roomId)
{
    return model->getRoomTemperature(roomId);
}

float GraphPresenter::getRoomHumidity(Rooms roomId)
{
    return model->getRoomHumidity(roomId);
}

HVAC_FanMode_t GraphPresenter::getRoomFanMode(Rooms roomId)
{
    return model->getRoomFanMode(roomId);
}

bool GraphPresenter::getIsFahrenheit()
{
    return model->getIsFahrenheit();
}

void GraphPresenter::setIsFahrenheit(bool isFahrenheit)
{
    model->setIsFahrenheit(isFahrenheit);
}

Rooms GraphPresenter::getSelectedRoom()
{
    return model->getSelectedRoom();
}

uint8_t GraphPresenter::getClockHour()
{
    return model->getClockHour();
}

uint8_t GraphPresenter::getClockMinute()
{
    return model->getClockMinute();
}

void GraphPresenter::getRoomBuffer(Rooms room, BufferSample returnBuffer[], uint8_t& returnBufferSize)
{
    return model->getRoomBuffer(room, returnBuffer, returnBufferSize);
}

void GraphPresenter::setSelectedRoom(Rooms room)
{
    model->setSelectedRoom(room);
}
