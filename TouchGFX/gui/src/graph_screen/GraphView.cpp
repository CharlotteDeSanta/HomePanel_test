#include <gui/graph_screen/GraphView.hpp>
#include <texts/TextKeysAndLanguages.hpp>
#include <images/BitmapDatabase.hpp>

GraphView::GraphView()
{

}

void GraphView::setupScreen()
{
    GraphViewBase::setupScreen();

    // Setup with values from model
    updateClock(presenter->getClockHour(), presenter->getClockMinute());

    if (presenter->getIsFahrenheit())
    {
        yTemperatureFahrenheitLabels.setVisible(true);
        yTemperatureLabels.setVisible(false);
        fahrenheitToggle.forceState(true);
    }

    switch (presenter->getSelectedRoom())
    {
    case KITCHEN:
        roomSelection.setSelected(kitchenButton);
        break;
    case LIVINGROOM:
        roomSelection.setSelected(livingRoomButton);
        break;
    case BEDROOM:
        roomSelection.setSelected(bedRoomButton);
        break;
    default:
        break;
    }

    // Set graph data
    BufferSample roomBuffer[MAX_BUFFER_SIZE];
    uint8_t roomBufferSize = 0;
    presenter->getRoomBuffer((Rooms)roomSelection.getSelectedRadioButtonIndex(), roomBuffer, roomBufferSize);
    setGraphData(roomBuffer, roomBufferSize);

    // Setup fade animations
    yFadeBox.setAlpha(255);
    xFadeBox.setAlpha(255);
    temperatureButton.setAlpha(0);
    fanButton.setAlpha(0);
    humidityButton.setAlpha(0);

    yFadeBox.startFadeAnimation(0, FADE_DURATION);
    xFadeBox.startFadeAnimation(0, FADE_DURATION);
}

void GraphView::tearDownScreen()
{
    GraphViewBase::tearDownScreen();
}

void GraphView::handleTickEvent()
{
    static int tickCount = 0;

    // Update performance text
    performanceText.updateMCULoad();
    performanceText.countTheFrames();

    // Fade new unit in, if old unit is fully faded out
    if (yFadeBox.getAlpha() == 255)
    {
        switch (unitSelection.getSelectedRadioButtonIndex())
        {
        case 0:     // Temperature selected
            if (presenter->getIsFahrenheit())
            {
                yTemperatureLabels.setVisible(false);
                yTemperatureFahrenheitLabels.setVisible(true);
            }
            else
            {
                yTemperatureLabels.setVisible(true);
                yTemperatureFahrenheitLabels.setVisible(false);
            }
            yFanSpeedLabels.setVisible(false);
            yHumidityLabels.setVisible(false);

            yFadeBox.startFadeAnimation(0, FADE_DURATION / 2);
            break;
        case 1:     // Fan selected
            yTemperatureLabels.setVisible(false);
            yTemperatureFahrenheitLabels.setVisible(false);
            yFanSpeedLabels.setVisible(true);
            yHumidityLabels.setVisible(false);

            yFadeBox.startFadeAnimation(0, FADE_DURATION / 2);
            break;
        case 2:     // Humidity selected
            yTemperatureLabels.setVisible(false);
            yTemperatureFahrenheitLabels.setVisible(false);
            yFanSpeedLabels.setVisible(false);
            yHumidityLabels.setVisible(true);

            yFadeBox.startFadeAnimation(0, FADE_DURATION / 2);
            break;
        default:
            break;
        }
    }

    // Fade in buttons in start animation
    if (xFadeBox.getAlpha() == 0 && temperatureButton.getAlpha() != 255)
    {
        temperatureButton.startFadeAnimation(255, FADE_DURATION);
        fanButton.startFadeAnimation(255, FADE_DURATION);
        humidityButton.startFadeAnimation(255, FADE_DURATION);
    }

    tickCount++;
}

void GraphView::temperatureButtonSelected()
{
    temperatureGraph.setAlpha(255);
    fanGraph.setAlpha(200);
    humidityGraph.setAlpha(200);

    remove(temperatureGraph);
    remove(fanGraph);
    remove(humidityGraph);
    add(humidityGraph);
    add(fanGraph);
    add(temperatureGraph);

    temperatureGraphLine1.setLineWidth(6);
    fanGraphLine1.setLineWidth(4);
    humidityGraphLine1.setLineWidth(4);

    temperatureGraph.invalidate();

    yFadeBox.startFadeAnimation(255, FADE_DURATION / 2);
}

void GraphView::fanButtonSelected()
{
    temperatureGraph.setAlpha(200);
    fanGraph.setAlpha(255);
    humidityGraph.setAlpha(200);

    remove(temperatureGraph);
    remove(fanGraph);
    remove(humidityGraph);
    add(humidityGraph);
    add(temperatureGraph);
    add(fanGraph);

    temperatureGraphLine1.setLineWidth(4);
    fanGraphLine1.setLineWidth(6);
    humidityGraphLine1.setLineWidth(4);

    temperatureGraph.invalidate();

    yFadeBox.startFadeAnimation(255, FADE_DURATION / 2);
}

void GraphView::humidityButtonSelected()
{
    temperatureGraph.setAlpha(200);
    fanGraph.setAlpha(200);
    humidityGraph.setAlpha(255);

    remove(temperatureGraph);
    remove(fanGraph);
    remove(humidityGraph);
    add(fanGraph);
    add(temperatureGraph);
    add(humidityGraph);

    temperatureGraphLine1.setLineWidth(4);
    fanGraphLine1.setLineWidth(4);
    humidityGraphLine1.setLineWidth(6);

    temperatureGraph.invalidate();

    yFadeBox.startFadeAnimation(255, FADE_DURATION / 2);
}

void GraphView::roomChanged()
{
    // Update selected room
    presenter->setSelectedRoom((Rooms)roomSelection.getSelectedRadioButtonIndex());

    // Set graph data
    BufferSample roomBuffer[MAX_BUFFER_SIZE];
    uint8_t roomBufferSize = 0;
    presenter->getRoomBuffer((Rooms)roomSelection.getSelectedRadioButtonIndex(), roomBuffer, roomBufferSize);
    setGraphData(roomBuffer, roomBufferSize);
}

void GraphView::fahrenheitToggleClicked()
{
    presenter->setIsFahrenheit(!presenter->getIsFahrenheit());

    if (unitSelection.getSelectedRadioButtonIndex() == 0)
    {
        yFadeBox.startFadeAnimation(255, FADE_DURATION / 2);
    }
}

void GraphView::updateClock(uint8_t hour, uint8_t minute)
{
    Unicode::snprintf(clockTextBuffer1, CLOCKTEXTBUFFER1_SIZE, "%02d", hour);
    Unicode::snprintf(clockTextBuffer2, CLOCKTEXTBUFFER2_SIZE, "%02d", minute);
    clockText.invalidate();
}

void GraphView::insertGraphSample(BufferSample graphSample)
{
    int fan = graphSample.fanMode;

    if (fan < 0 || fan > HVAC_FAN_HIGH)
    {
        fan = 0;
    }

    temperatureGraph.addDataPoint(graphSample.temperature);
    humidityGraph.addDataPoint(graphSample.humidity);
    fanGraph.addDataPoint(fan);
}

void GraphView::setGraphData(BufferSample roomBuffer[], uint8_t roomBufferSize)
{
    temperatureGraph.clear();
    fanGraph.clear();
    humidityGraph.clear();

    for (uint8_t i = 0; i < roomBufferSize; i++)
    {
        int fanMode = roomBuffer[i].fanMode;
        if (fanMode < 0 || fanMode > HVAC_FAN_HIGH)
        {
            fanMode = 0;
        }

        temperatureGraph.addDataPoint(roomBuffer[i].temperature);
        fanGraph.addDataPoint(fanMode);
        humidityGraph.addDataPoint(roomBuffer[i].humidity);
    }
}
