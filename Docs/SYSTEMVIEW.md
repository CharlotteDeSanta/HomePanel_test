# SEGGER SystemView Quick Start (HomePanel_test)

This project can stream FreeRTOS runtime trace to SEGGER SystemView over RTT.

## 1) Build-time switch

SystemView is controlled by CMake option:

- `APP_ENABLE_SYSTEMVIEW=ON` (default): build with SystemView + RTT
- `APP_ENABLE_SYSTEMVIEW=OFF`: build without SystemView

The option is defined in:

- `C:/Users/20953/Documents/MCUProjects/HomePanel_test/CMakeLists.txt`

## 2) Required host tools

Install on PC:

- SEGGER J-Link package
- SEGGER SystemView

Use a J-Link probe to connect to STM32H743 target.

## 3) Target-side integration in this project

Integrated source roots:

- `C:/Users/20953/Documents/MCUProjects/HomePanel_test/BSP/ThirdParty/SEGGER/SystemView`
- `C:/Users/20953/Documents/MCUProjects/HomePanel_test/BSP/ThirdParty/SEGGER/RTT`

FreeRTOS trace hook include:

- `C:/Users/20953/Documents/MCUProjects/HomePanel_test/Core/Inc/FreeRTOSConfig.h`

Runtime init point:

- `C:/Users/20953/Documents/MCUProjects/HomePanel_test/Core/Src/freertos.c`

## 4) Start recording in SystemView

1. Flash and run firmware.
2. Open SystemView.
3. Select target device `STM32H743`.
4. Connection type: J-Link SWD.
5. Set CPU frequency to match firmware clock (must match `SystemCoreClock` used by FreeRTOS).
6. Start recording.

If no events appear:

- Confirm firmware is running (not halted).
- Confirm SWD/J-Link connection is stable.
- Confirm build used `APP_ENABLE_SYSTEMVIEW=ON`.

## 5) Recommended debug workflow

- Keep UART logs minimal when tracing (avoid extra CPU overhead).
- Use SystemView to inspect:
  - task scheduling
  - ISR burst timing
  - queue/semaphore hot spots
  - long critical sections

## 6) Notes

- This project initializes SystemView before creating tasks, so early RTOS events are visible.
- RTT lock priority is aligned with FreeRTOS syscall interrupt priority for safer concurrent trace logging.
