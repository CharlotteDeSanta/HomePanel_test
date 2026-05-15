#include <gui/main_screen/MainView.hpp>
#include <texts/TextKeysAndLanguages.hpp>

MainView::MainView() :
    tickCount(0),
    waitingToFlip(false),
    setTempAndFanUsbSettingChangedCallback(this, &MainView::usbSettingsChanged),
    roomStatusNoopCallback(this, &MainView::roomStatusNoopHandler)
{
    setTempAndFan.setUsbSettingChangedCallback(setTempAndFanUsbSettingChangedCallback);
}

void MainView::setupScreen()
{
    MainViewBase::setupScreen();

    closeSettingsButton.setTouchable(false);

    // Setup with values from model
    updateClock(presenter->getClockHour(), presenter->getClockMinute());
    setTemperatures();
    updateRoomHumidity(KITCHEN, presenter->getRoomHumidity(KITCHEN));
    updateRoomHumidity(LIVINGROOM, presenter->getRoomHumidity(LIVINGROOM));
    updateRoomHumidity(BEDROOM, presenter->getRoomHumidity(BEDROOM));
    kitchenFan.setFanMode(presenter->getRoomFanMode(KITCHEN));
    livingRoomFan.setFanMode(presenter->getRoomFanMode(LIVINGROOM));
    bedRoomFan.setFanMode(presenter->getRoomFanMode(BEDROOM));
    setTempAndFan.setUsbFlags(KITCHEN, presenter->getRoomUsbFlags(KITCHEN));
    setTempAndFan.setUsbFlags(LIVINGROOM, presenter->getRoomUsbFlags(LIVINGROOM));
    setTempAndFan.setUsbFlags(BEDROOM, presenter->getRoomUsbFlags(BEDROOM));
    kitchenCardContainer.setFanSetPoint(presenter->getRoomFanSetPoint(KITCHEN));
    livingroomCardContainer.setFanSetPoint(presenter->getRoomFanSetPoint(LIVINGROOM));
    bedroomCardContainer.setFanSetPoint(presenter->getRoomFanSetPoint(BEDROOM));
    kitchenStat.setTouchable(false);
    livingStat.setTouchable(false);
    bedroomStat.setTouchable(false);
    kitchenStat.setAction(roomStatusNoopCallback);
    livingStat.setAction(roomStatusNoopCallback);
    bedroomStat.setAction(roomStatusNoopCallback);
    wifistatText.setTypedText(TypedText(T_ENTEREDTEXT));
    updateWiFiOnline(presenter->getWiFiOnline());
    updateRoomOnline(KITCHEN, presenter->getRoomOnline(KITCHEN));
    updateRoomOnline(LIVINGROOM, presenter->getRoomOnline(LIVINGROOM));
    updateRoomOnline(BEDROOM, presenter->getRoomOnline(BEDROOM));

    // Setup start animations
    startupAnimation(wificonfButton, FADE_DURATION, 0);
    startupAnimation(wificonfIcon, FADE_DURATION, 0);
    kitchenCardContainer.startupAnimation(FADE_DURATION, 1, TypedText(T_KITCHENTEXT));
    livingroomCardContainer.startupAnimation(FADE_DURATION, 2, TypedText(T_LIVINGROOMTEXT));
    bedroomCardContainer.startupAnimation(FADE_DURATION, 3, TypedText(T_BEDROOMTEXT));

    // Disable room card buttons until animation is done
    kitchenCardButton.setTouchable(false);
    livingroomCardButton.setTouchable(false);
    bedroomCardButton.setTouchable(false);
}

void MainView::tearDownScreen()
{
    MainViewBase::tearDownScreen();
}

void MainView::handleTickEvent()
{
    tickCount++;

    // Enable room card buttons when animation is done
    if (tickCount == FADE_DURATION * 4)
    {
        kitchenCardButton.setTouchable(true);
        livingroomCardButton.setTouchable(true);
        bedroomCardButton.setTouchable(true);
    }

    // Setup room card start animations when returning from setTempAndFan
    if (waitingToFlip && !(livingroomButtonContainer.isMoveAnimationRunning() || bedroomButtonContainer.isMoveAnimationRunning()))
    {
        waitingToFlip = false;

        switch (presenter->getSelectedRoom())
        {
        case KITCHEN:
            livingroomCardContainer.startupAnimation(FADE_DURATION, 1, TypedText(T_LIVINGROOMTEXT));
            bedroomCardContainer.startupAnimation(FADE_DURATION, 2, TypedText(T_BEDROOMTEXT));
            break;
        case LIVINGROOM:
            kitchenCardContainer.startupAnimation(FADE_DURATION, 1, TypedText(T_KITCHENTEXT));
            bedroomCardContainer.startupAnimation(FADE_DURATION, 2, TypedText(T_BEDROOMTEXT));
            break;
        case BEDROOM:
            kitchenCardContainer.startupAnimation(FADE_DURATION, 1, TypedText(T_KITCHENTEXT));
            livingroomCardContainer.startupAnimation(FADE_DURATION, 2, TypedText(T_LIVINGROOMTEXT));
            break;
        default:
            break;
        }
    }

    // Update performance text
    performanceText.updateMCULoad();
    performanceText.countTheFrames();
}

void MainView::kitchenCardButtonClicked()
{
    livingroomCardContainer.flipOut(FLIP_DURATION);
    bedroomCardContainer.flipOut(FLIP_DURATION);
    setTempAndFan.setUsbFlags(KITCHEN, presenter->getRoomUsbFlags(KITCHEN));
    setTempAndFan.showContainer(KITCHEN, presenter->getRoomFanSetPoint(KITCHEN), presenter->getRoomTempSetPoint(KITCHEN), presenter->getIsFahrenheit());

    setSelectedRoom(KITCHEN);
}

void MainView::livingroomCardButtonClicked()
{
    kitchenCardContainer.flipOut(FLIP_DURATION);
    bedroomCardContainer.flipOut(FLIP_DURATION);
    setTempAndFan.setUsbFlags(LIVINGROOM, presenter->getRoomUsbFlags(LIVINGROOM));
    setTempAndFan.showContainer(LIVINGROOM, presenter->getRoomFanSetPoint(LIVINGROOM), presenter->getRoomTempSetPoint(LIVINGROOM), presenter->getIsFahrenheit());

    setSelectedRoom(LIVINGROOM);
}

void MainView::bedroomCardButtonClicked()
{
    kitchenCardContainer.flipOut(FLIP_DURATION);
    livingroomCardContainer.flipOut(FLIP_DURATION);
    setTempAndFan.setUsbFlags(BEDROOM, presenter->getRoomUsbFlags(BEDROOM));
    setTempAndFan.showContainer(BEDROOM, presenter->getRoomFanSetPoint(BEDROOM), presenter->getRoomTempSetPoint(BEDROOM), presenter->getIsFahrenheit());

    setSelectedRoom(BEDROOM);
}

void MainView::resetRoomCards()
{
    kitchenCardButton.setTouchable(false);
    livingroomCardButton.setTouchable(false);
    bedroomCardButton.setTouchable(false);
    tickCount = 0;

    switch (presenter->getSelectedRoom())
    {
    case LIVINGROOM:
        livingroomButtonContainer.setMoveAnimationDelay(0);
        livingroomButtonContainer.startMoveAnimation(CARD_X, LIVINGROOM_Y, 8);
        break;
    case BEDROOM:
        bedroomButtonContainer.setMoveAnimationDelay(0);
        bedroomButtonContainer.startMoveAnimation(CARD_X, BEDROOM_Y, 16);
        break;
    default:
        break;
    }

    waitingToFlip = true;

    temperatureAnimation.startAnimation(presenter->getSelectedRoom(), presenter->getRoomTempSetPoint(presenter->getSelectedRoom()), presenter->getRoomTemperature(presenter->getSelectedRoom()), presenter->getIsFahrenheit());
}

void MainView::fanModeChanged(HVAC_FanMode_t fanMode)
{
    presenter->setRoomFanSetPoint(presenter->getSelectedRoom(), fanMode);

    switch (presenter->getSelectedRoom())
    {
    case KITCHEN:
        kitchenCardContainer.setFanSetPoint(fanMode);
        break;
    case LIVINGROOM:
        livingroomCardContainer.setFanSetPoint(fanMode);
        break;
    case BEDROOM:
        bedroomCardContainer.setFanSetPoint(fanMode);
        break;
    default:
        break;
    }
}

void MainView::usbSettingsChanged(uint8_t flags)
{
    presenter->setRoomUsbFlags(presenter->getSelectedRoom(), flags);
}

void MainView::temperatureChanged(float temperature)
{
    presenter->setRoomTempSetPoint(presenter->getSelectedRoom(), temperature);
    closeSettingsButton.setTouchable(false);
}

void MainView::temperatureAnimationDone()
{
    kitchenSelectedImage.startFadeAnimation(0, FADE_DURATION);
    livingroomSelectedImage.startFadeAnimation(0, FADE_DURATION);
    bedroomSelectedImage.startFadeAnimation(0, FADE_DURATION);
}

void MainView::graphButtonClicked(Rooms room)
{
    presenter->setSelectedRoom(room);
    application().gotoGraphScreenNoTransition();
}

void MainView::updateRoomTemperature(Rooms roomId, float temperature)
{
    switch (roomId)
    {
    case KITCHEN:
        kitchenCardContainer.setTemperature(temperature, presenter->getIsFahrenheit());
        break;
    case LIVINGROOM:
        livingroomCardContainer.setTemperature(temperature, presenter->getIsFahrenheit());
        break;
    case BEDROOM:
        bedroomCardContainer.setTemperature(temperature, presenter->getIsFahrenheit());
        break;
    default:
        break;
    }
}

void MainView::updateRoomHumidity(Rooms roomId, float humidity)
{
    switch (roomId)
    {
    case KITCHEN:
        kitchenCardContainer.setHumidity(humidity);
        break;
    case LIVINGROOM:
        livingroomCardContainer.setHumidity(humidity);
        break;
    case BEDROOM:
        bedroomCardContainer.setHumidity(humidity);
        break;
    default:
        break;
    }
}

void MainView::updateRoomFanMode(Rooms roomId, HVAC_FanMode_t fanMode)
{
    switch (roomId)
    {
    case KITCHEN:
        kitchenFan.setFanMode(fanMode);
        break;
    case LIVINGROOM:
        livingRoomFan.setFanMode(fanMode);
        break;
    case BEDROOM:
        bedRoomFan.setFanMode(fanMode);
        break;
    default:
        break;
    }
}

void MainView::updateRoomUsbFlags(Rooms roomId, uint8_t usbFlags)
{
    setTempAndFan.setUsbFlags(roomId, usbFlags);
}

void MainView::updateWiFiOnline(bool online)
{
    const char* status = online ? "Online" : "Offline";
    Unicode::strncpy(wifistatTextBuffer, status, WIFISTATTEXT_SIZE);
    wifistatTextBuffer[WIFISTATTEXT_SIZE - 1U] = 0;
    wifistatText.resizeToCurrentText();
    wifistatText.invalidate();
}

void MainView::updateRoomOnline(Rooms roomId, bool online)
{
    switch (roomId)
    {
    case KITCHEN:
        kitchenStat.setSelected(online);
        kitchenStat.invalidate();
        break;
    case LIVINGROOM:
        livingStat.setSelected(online);
        livingStat.invalidate();
        break;
    case BEDROOM:
        bedroomStat.setSelected(online);
        bedroomStat.invalidate();
        break;
    default:
        break;
    }
}

void MainView::roomStatusNoopHandler(const touchgfx::AbstractButton&)
{
}

void MainView::updateClock(uint8_t hour, uint8_t minute)
{
    Unicode::snprintf(clockTextBuffer1, CLOCKTEXTBUFFER1_SIZE, "%02d/%02d/%04d %02d",
                      presenter->getClockMonth(),
                      presenter->getClockDay(),
                      presenter->getClockYear(),
                      hour);
    Unicode::snprintf(clockTextBuffer2, CLOCKTEXTBUFFER2_SIZE, "%02d", minute);
    clockText.invalidate();
}

void MainView::updateDate(uint16_t year, uint8_t month, uint8_t day, uint8_t weekday)
{
    (void)year;
    (void)month;
    (void)day;
    (void)weekday;

    updateClock(presenter->getClockHour(), presenter->getClockMinute());
}

void MainView::updateCurrentWeather(WeatherData weatherData, bool isUnitFahrenheit)
{
    (void)weatherData;
    (void)isUnitFahrenheit;
}

void MainView::setTemperatures()
{
    // Set temperatures in correct unit
    kitchenCardContainer.setTemperature(presenter->getRoomTemperature(KITCHEN), presenter->getIsFahrenheit());
    livingroomCardContainer.setTemperature(presenter->getRoomTemperature(LIVINGROOM), presenter->getIsFahrenheit());
    bedroomCardContainer.setTemperature(presenter->getRoomTemperature(BEDROOM), presenter->getIsFahrenheit());

    temperatureAnimation.setTemperature(presenter->getRoomTempSetPoint(presenter->getSelectedRoom()), presenter->getIsFahrenheit());
}

void MainView::setSelectedRoom(Rooms room)
{
    temperatureAnimationDone();

    // Update model
    presenter->setSelectedRoom(room);

    // Stop animations already running
    if (kitchenSelectedImage.getAlpha() || livingroomSelectedImage.getAlpha() || bedroomSelectedImage.getAlpha())
    {
        kitchenSelectedImage.startFadeAnimation(0, FADE_DURATION);
        livingroomSelectedImage.startFadeAnimation(0, FADE_DURATION);
        bedroomSelectedImage.startFadeAnimation(0, FADE_DURATION);

        temperatureAnimation.stopAnimation();
    }

    // Set highlight on selected room
    switch (room)
    {
    case KITCHEN:
        kitchenSelectedImage.startFadeAnimation(255, FADE_DURATION);
        break;
    case LIVINGROOM:
        livingroomSelectedImage.startFadeAnimation(255, FADE_DURATION);
        break;
    case BEDROOM:
        bedroomSelectedImage.startFadeAnimation(255, FADE_DURATION);
        break;
    default:
        break;
    }

    // Disable card buttons
    kitchenCardButton.setTouchable(false);
    livingroomCardButton.setTouchable(false);
    bedroomCardButton.setTouchable(false);
}
