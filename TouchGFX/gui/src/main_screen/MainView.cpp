#include <gui/main_screen/MainView.hpp>
#include <texts/TextKeysAndLanguages.hpp>

/* Construct main screen view and wire callbacks used by settings/indicator widgets. */
MainView::MainView() :
    tickCount(0),
    waitingToFlip(false),
    setTempAndFanUsbSettingChangedCallback(this, &MainView::usbSettingsChanged),
    roomStatusNoopCallback(this, &MainView::roomStatusNoopHandler)
{
    setTempAndFan.setUsbSettingChangedCallback(setTempAndFanUsbSettingChangedCallback);
}

/* Initialize the main screen widgets and bind initial state from presenter/model. */
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
    /* Online indicators are display-only; state comes from model callbacks. */
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

/* Tear down main screen resources when navigating away from this view. */
void MainView::tearDownScreen()
{
    MainViewBase::tearDownScreen();
}

/* Drive startup/transition animations and periodic performance text updates. */
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

/* Enter settings view with Kitchen as current room context. */
void MainView::kitchenCardButtonClicked()
{
    livingroomCardContainer.flipOut(FLIP_DURATION);
    bedroomCardContainer.flipOut(FLIP_DURATION);
    setTempAndFan.setUsbFlags(KITCHEN, presenter->getRoomUsbFlags(KITCHEN));
    setTempAndFan.showContainer(KITCHEN, presenter->getRoomFanSetPoint(KITCHEN), presenter->getRoomTempSetPoint(KITCHEN), presenter->getIsFahrenheit());

    setSelectedRoom(KITCHEN);
}

/* Enter settings view with Living Room as current room context. */
void MainView::livingroomCardButtonClicked()
{
    kitchenCardContainer.flipOut(FLIP_DURATION);
    bedroomCardContainer.flipOut(FLIP_DURATION);
    setTempAndFan.setUsbFlags(LIVINGROOM, presenter->getRoomUsbFlags(LIVINGROOM));
    setTempAndFan.showContainer(LIVINGROOM, presenter->getRoomFanSetPoint(LIVINGROOM), presenter->getRoomTempSetPoint(LIVINGROOM), presenter->getIsFahrenheit());

    setSelectedRoom(LIVINGROOM);
}

/* Enter settings view with Bedroom as current room context. */
void MainView::bedroomCardButtonClicked()
{
    kitchenCardContainer.flipOut(FLIP_DURATION);
    livingroomCardContainer.flipOut(FLIP_DURATION);
    setTempAndFan.setUsbFlags(BEDROOM, presenter->getRoomUsbFlags(BEDROOM));
    setTempAndFan.showContainer(BEDROOM, presenter->getRoomFanSetPoint(BEDROOM), presenter->getRoomTempSetPoint(BEDROOM), presenter->getIsFahrenheit());

    setSelectedRoom(BEDROOM);
}

/* Restore room cards after leaving settings panel and restart temp animation context. */
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

/* Handle fan setpoint changes from SetTempAndFan panel and mirror to room card. */
void MainView::fanModeChanged(HVAC_FanMode_t fanMode)
{
    /* UI -> presenter -> model -> backend control queue. */
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

/* Handle USB output flag changes from SetTempAndFan panel. */
void MainView::usbSettingsChanged(uint8_t flags)
{
    /* Push current room USB mask through the same presenter/model control path. */
    presenter->setRoomUsbFlags(presenter->getSelectedRoom(), flags);
}

/* Handle target temperature setpoint change from SetTempAndFan panel. */
void MainView::temperatureChanged(float temperature)
{
    /* Temperature setpoint write is filtered by model policy (AUTO mode only). */
    presenter->setRoomTempSetPoint(presenter->getSelectedRoom(), temperature);
    closeSettingsButton.setTouchable(false);
}

/* Fade out room highlight overlays when the temp animation phase completes. */
void MainView::temperatureAnimationDone()
{
    kitchenSelectedImage.startFadeAnimation(0, FADE_DURATION);
    livingroomSelectedImage.startFadeAnimation(0, FADE_DURATION);
    bedroomSelectedImage.startFadeAnimation(0, FADE_DURATION);
}

/* Navigate to graph screen for the requested room context. */
void MainView::graphButtonClicked(Rooms room)
{
    presenter->setSelectedRoom(room);
    application().gotoGraphScreenNoTransition();
}

/* Apply live room temperature telemetry to the corresponding room card widget. */
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

/* Apply live room humidity telemetry to the corresponding room card widget. */
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

/* Apply live room fan runtime mode telemetry to fan indicators. */
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

/* Reflect USB output state from model into SetTempAndFan panel state. */
void MainView::updateRoomUsbFlags(Rooms roomId, uint8_t usbFlags)
{
    setTempAndFan.setUsbFlags(roomId, usbFlags);
}

/* Update WiFi connectivity status wildcard text shown on the main screen. */
void MainView::updateWiFiOnline(bool online)
{
    /* Wildcard text binding for designer text area (ASCII-safe "Online/Offline"). */
    const char* status = online ? "Online" : "Offline";
    Unicode::strncpy(wifistatTextBuffer, status, WIFISTATTEXT_SIZE);
    wifistatTextBuffer[WIFISTATTEXT_SIZE - 1U] = 0;
    wifistatText.resizeToCurrentText();
    wifistatText.invalidate();
}

/* Update passive room online indicators based on backend node liveness state. */
void MainView::updateRoomOnline(Rooms roomId, bool online)
{
    /* Room online radio buttons are passive indicators controlled by model state. */
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

/* No-op action sink for non-interactive room online indicators. */
void MainView::roomStatusNoopHandler(const touchgfx::AbstractButton&)
{
}

/* Render current clock/date text using model-provided date components and time. */
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

/* Date callback forwards to unified clock renderer to keep one formatting path. */
void MainView::updateDate(uint16_t year, uint8_t month, uint8_t day, uint8_t weekday)
{
    (void)year;
    (void)month;
    (void)day;
    (void)weekday;

    updateClock(presenter->getClockHour(), presenter->getClockMinute());
}

/* Weather hook retained for compatibility; current design does not render weather on main cards. */
void MainView::updateCurrentWeather(WeatherData weatherData, bool isUnitFahrenheit)
{
    (void)weatherData;
    (void)isUnitFahrenheit;
}

/* Refresh room card temperatures and settings panel temperature in selected unit. */
void MainView::setTemperatures()
{
    // Set temperatures in correct unit
    kitchenCardContainer.setTemperature(presenter->getRoomTemperature(KITCHEN), presenter->getIsFahrenheit());
    livingroomCardContainer.setTemperature(presenter->getRoomTemperature(LIVINGROOM), presenter->getIsFahrenheit());
    bedroomCardContainer.setTemperature(presenter->getRoomTemperature(BEDROOM), presenter->getIsFahrenheit());

    temperatureAnimation.setTemperature(presenter->getRoomTempSetPoint(presenter->getSelectedRoom()), presenter->getIsFahrenheit());
}

/* Update selected room context and synchronize card highlight/interaction state. */
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
