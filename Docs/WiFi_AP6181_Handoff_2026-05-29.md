# AP6181 WiFi/LwIP Stability Handoff

Date: 2026-05-29

This note summarizes the changes and reasoning made while debugging the upper controller
`HomePanel_test` and lower-node project `HomeNode`, starting from the SDIO clock adjustment.

## Projects

- Upper controller: `C:\Users\20953\Documents\MCUProjects\HomePanel_test`
- Lower node: `C:\Users\20953\Documents\MCUProjects\HomeNode`

## Observed Problem

- With no lower nodes powered, the upper controller can stay associated with WiFi.
- With one or two lower nodes, control is mostly stable after recent changes.
- With three lower nodes, all three can now connect, but bidirectional traffic still tends to cause:
  - increasing control latency,
  - failed or delayed telemetry/state sync,
  - `txq` growth on the upper controller,
  - eventual `[wifi] join: link down status=0 reason=1 flags=0x0000`.
- The last representative log showed:
  - `control tx #1` at `txq=5`
  - `control tx #2` at `txq=7`
  - `control tx #3` at `txq=13`
  - a previous run linked down around `txq=17`.
- `txq_fail=0`, `rx_pbuf_fail=0`, `rx_input_fail=0`, and heap was not exhausted. This makes a simple lwIP heap/pbuf exhaustion diagnosis unlikely.
- Phone hotspot was worse than campus WiFi. Campus WiFi scan shows the same SSID on multiple channels/BSSIDs, so roaming/BSSID churn should be considered.

## Current Working Conclusion

The software changes improved correctness and reduced obvious traffic spikes, but the remaining failure looks more like a platform/network limit:

- AP6181/BCM43362 firmware or SDIO path becomes unstable under three TCP clients with small bidirectional frames.
- The upper controller's WiFi link is sensitive to aggregate TX/RX load, not just RF signal strength.
- Further small timing tweaks may delay the failure but are unlikely to be a complete fix.

## Upper Controller Changes

### 1. SDIO clock split into enumeration and runtime clocks

File:

- `Core/Src/app_wifi_platform.c`

Changes:

- Added:
  - `APP_WIFI_SDIO_ENUM_CLK_DIV 300U`
  - `APP_WIFI_SDIO_RUN_CLK_DIV 10U`
- Changed `APP_WiFi_Platform_ApplyHostBusWidth()` to accept a clock divider.
- Enumeration uses slow 1-bit mode.
- Runtime uses 4-bit mode with clock divider 10.

Intent:

- Keep enumeration conservative.
- Reduce runtime SDIO frequency stress while keeping throughput usable.
- With the current STM32 SDMMC clock assumption, divider 10 is approximately 12 MHz.

Notes for engineer:

- Verify actual SDMMC kernel clock and resulting SDIO clock on hardware.
- Probe SDIO CLK/CMD/DAT lines if possible.
- Consider testing lower runtime clocks, 1-bit runtime mode, pull-up strength, trace length, and signal integrity.

### 2. SDPCM empty interrupt handling

File:

- `Core/Src/app_wifi.c`

Changes:

- Kept `APP_WIFI_SDPCM_FRAME_AVAILABLE_MASK` at `0x000000F0U`.
- Added `APP_WIFI_SDPCM_EMPTY_IRQ_MASK` as `0x00000040U`.
- If SDPCM header has `frameLength == 0` and `frameCheck == 0`, return without treating it as a hwtag mismatch.
- If the interrupt hint is exactly empty IRQ `0x40`, clear the function-2 RX interrupt.

Intent:

- Suppress false `sdpcm: hwtag mismatch irq=0x40 len=0 chk=0 raw=00...` spam.
- Avoid breaking DHCP/credits. A previous experiment that narrowed the available-frame mask too aggressively suppressed the mismatch log but caused DHCP stalls and TX busy buildup.

### 3. Link-up after transient down now requests network rebind

File:

- `Core/Src/app_wifi.c`

Change:

- On link-up after transient link issue, changed behavior from:
  - `APP_WiFi_LwIP_RequestSessionRefresh()`
- to:
  - `APP_WiFi_LwIP_RequestNetworkRebind()`

Intent:

- Rebuild lwIP netif/DHCP/server state more completely after AP6181 reports a transient link flap.

### 4. Upper lwIP/control TX throttling

File:

- `Core/Src/app_wifi_lwip.c`

Changes:

- Reduced `APP_WIFI_LWIP_TX_BURST_LIMIT` from `24U` to `8U`.
- Added `APP_WIFI_LWIP_CONTROL_GLOBAL_TX_GAP_MS 180U`.
- Added `g_nextGlobalControlTxTick`.
- Applied the global control TX gap in both:
  - `APP_WiFi_LwIP_DrainAllPendingControls()`
  - `APP_WiFi_LwIP_DrainPendingControls()`
- Reset global gate in `APP_WiFi_LwIP_ResetControlState()`.

Intent:

- Avoid sending controls to three active sockets almost simultaneously.
- Prevent client tasks from competing and producing a sudden AP6181/lwIP TX burst.
- Reduce `txq` growth before linkdown.

Result:

- Still not enough to fully stabilize the three-node case.
- Latest log still reached `txq=13` during three consecutive control sends.

### 5. lwIP sanity check fix

File:

- `Core/Inc/lwipopts.h`

Change:

- `TCP_SNDQUEUELOWAT` changed from `2` to `1`.

Reason:

- With current `TCP_SND_QUEUELEN`, lwIP's sanity check rejected `TCP_SNDQUEUELOWAT >= TCP_SND_QUEUELEN`.

### 6. Build artifact generation

File:

- `CMakeLists.txt`

Change:

- Added post-build `objcopy` commands to generate:
  - `HomePanel_test.hex`
  - `HomePanel_test.bin`

Output location:

- Same directory as the ELF, for example `build/Release/`.

## Lower Node Changes

### 1. TCP connect state machine no longer probes CIPSEND after ambiguous CIPSTART

File:

- `App/Src/app_node.c` in `HomeNode`

Changes:

- Removed `APP_ESP_STATE_UPLOAD_CIPSTART_SETTLE`.
- Removed broad `app_esp_cipstart_likely_connected()` heuristic.
- `CIPSTART` only proceeds to `CIPSEND` when `app_esp_step_exchange()` returns explicit success:
  - expected `OK`, or
  - expected `ALREADY CONNECTED`.
- If `CIPSTART` has no final OK and no explicit error, it now logs:
  - `CIPSTART no final OK, close and retry later`
- Then calls `app_schedule_cipstart_retry()`.

Intent:

- Previous behavior assumed ambiguous `CIPSTART` may have succeeded and then sent `CIPSEND`.
- Lower-node logs showed this caused ESP AT busy loops:
  - `CIPSTART no final OK, wait then probe CIPSEND`
  - `busy p...`
  - `CIPSEND prompt timeout, recover TCP`
- New behavior avoids pushing data into an uncertain ESP TCP state.

### 2. TCP retry backoff widened

File:

- `App/Src/app_node.c`

Changes:

- `APP_CIPSTART_FAIL_RETRY_BASE_MS`: `1200U` to `2500U`
- `APP_CIPSTART_FAIL_RETRY_MAX_MS`: `7000U` to `15000U`
- `APP_CIPSTART_FAIL_RETRY_JITTER_MS`: `400U` to `1000U`

Intent:

- Avoid repeated simultaneous reconnect attempts when several nodes power on or recover together.

### 3. Node start and upload staggering

File:

- `App/Src/app_node.c`

Changes:

- Added `APP_ESP_POST_IP_NODE_STAGGER_MS 2500U`.
- Added `app_node_tcp_start_stagger_ms()`.
- After CIFSR/IP confirmation, TCP upload starts after:
  - `APP_ESP_POST_IP_UPLOAD_DELAY_MS + (APP_NODE_ID - 1) * 2500ms`
- Initial `s_nextUploadAttemptTick` now uses the same node-based stagger.

Intent:

- Avoid all lower nodes opening TCP immediately after WiFi/IP comes up.

### 4. State-upload staggering after control apply

File:

- `App/Src/app_node.c`

Changes:

- `APP_UPLOAD_SCHEDULE_JITTER_MS`: currently `1800U`
- `APP_STATE_UPLOAD_DEBOUNCE_MS`: currently `800U`
- Added `APP_STATE_UPLOAD_NODE_STAGGER_MS 900U`.
- Added `app_node_state_upload_stagger_ms()`.
- State upload after control is scheduled after:
  - `800ms + (APP_NODE_ID - 1) * 900ms`

Intent:

- Avoid all nodes immediately sending telemetry/state echo after upper-controller commands.
- Reduce simultaneous uplink traffic after a multi-room control test.

### 5. ACK/ERR reply path currently disabled

File:

- `App/Src/app_node.c`

Current behavior:

- `app_queue_protocol_ack()` and `app_queue_protocol_error()` are no-ops.
- Control confirmation is expected through telemetry/state upload instead of explicit ACK/ERR.

Intent:

- Reduce protocol chatter and avoid ACK/ERR traffic amplifying the fragile bidirectional load.

Note:

- This behavior existed in the current working tree during this debugging sequence and should be reviewed by the engineer. It is useful for load reduction, but it changes protocol semantics.

### 6. Current lower-node upload endpoint

File:

- `App/Src/app_node.c`

Current value:

- `APP_UPLOAD_HOST "10.20.250.232"`
- `APP_UPLOAD_PORT 5000U`

Note:

- This is the upper controller address seen during the tests.

## Current Test Status

Improvements observed:

- Three lower nodes can connect to the upper controller.
- Upper controller can transmit control frames to node 1, node 2, and node 3.
- False SDPCM empty-frame `hwtag mismatch` logs are reduced without breaking DHCP.
- DHCP can bind after SDIO empty IRQ handling was corrected.

Still failing:

- Three-node bidirectional traffic still causes `txq` growth and eventually linkdown.
- The failure is not accompanied by obvious `txq_fail`, `rx_pbuf_fail`, or heap exhaustion in the logs.
- Two-node operation remains much more stable than three-node operation.

## Recommended Engineering Investigation

### Hardware and SDIO

- Measure actual SDIO clock after `APP_WIFI_SDIO_RUN_CLK_DIV`.
- Probe SDIO CLK/CMD/DAT signals under three-node traffic.
- Test lower runtime SDIO clocks and possibly 1-bit runtime mode as a diagnostic.
- Check AP6181 power rail droop during TX bursts.
- Check SDIO pull-ups, trace impedance/length, and GPIO speed/drive configuration.

### WiFi/AP environment

- Use a dedicated 2.4 GHz router/AP for testing, not campus WiFi or phone hotspot.
- Use a unique SSID on one fixed channel.
- Avoid same SSID across multiple BSSIDs/channels during diagnosis.
- If AP6181 firmware supports it, consider locking BSSID/channel to prevent roaming decisions.

### Firmware/driver

- Review AP6181/BCM43362 firmware and NVRAM settings.
- Confirm `PM_OFF`, scan suppress, tx aggregation/glom settings, country/gmode, and APSTA settings are appropriate.
- Add max/high-watermark logs for:
  - `txq`
  - SDIO errors
  - no-credit stalls
  - firmware async link/auth/disassoc events
  - AP BSSID/channel before and after linkdown

### Architecture fallback

If AP6181 cannot reliably handle three concurrent TCP clients:

- Consider UDP for node telemetry/control with application-level sequence/ack.
- Consider one lower node connection at a time, or a polling model.
- Consider moving WiFi responsibility to a more capable module/MCU.
- Consider upper controller as a dedicated AP or using a wired/serial gateway.

## Important Files to Review

Upper:

- `Core/Src/app_wifi_platform.c`
- `Core/Src/app_wifi.c`
- `Core/Src/app_wifi_lwip.c`
- `Core/Inc/lwipopts.h`
- `CMakeLists.txt`

Lower:

- `C:\Users\20953\Documents\MCUProjects\HomeNode\App\Src\app_node.c`

