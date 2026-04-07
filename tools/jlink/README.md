# J-Link QSPI download setup

This project uses external QSPI flash at `0x90000000` for TouchGFX image
resources. Programming this area requires a dedicated J-Link flash loader.
The current setup targets a single `W25Q256` on the working BK0 QSPI pins.

## Files in this folder

- `JLinkDevices/ST/STM32H7/Devices.xml`
  Adds a QSPI flash bank for `STM32H743XI`
- `JLinkDevices/ST/STM32H7/loaders/README.txt`
  Explains which SEGGER loader file to download

## One-time setup

1. Install the SEGGER J-Link Software Pack.
2. Download the loader file from:
   `https://kb.segger.com/STM32H753I-EVAL`
3. Save the single-flash loader as:
   `tools/jlink/JLinkDevices/ST/STM32H7/loaders/ST_STM32H753I_EVAL_QSPI.elf`
4. Install the VS Code extension:
   `Cortex-Debug` by `marus25`

## Recommended workflow

1. Build the project as usual.
2. Use the VS Code launch config:
   `J-Link Program + Debug (Custom QSPI Loader)`
   This starts J-Link GDB Server with:
   `-JLinkDevicesXMLPath ${workspaceFolder}/tools/jlink/JLinkDevices`
3. After a full successful download, use:
   `J-Link Attach Only (No Load)`
   for normal debug sessions.

## Fallback if the IDE ignores JLinkDevicesXMLPath

Copy this folder:

`tools/jlink/JLinkDevices`

to the SEGGER per-user device directory:

`C:/Users/<YOUR_USER>/AppData/Roaming/SEGGER/JLinkDevices`

SEGGER documents this folder as a standard search location for custom
`JLinkDevices.xml` files.

## Notes

- This project is currently left in a temporary QSPI diagnostic mode.
- Once external flash programming is confirmed to work, the diagnostic changes
  in the firmware should be reverted and TouchGFX restored to the normal flow.
- Do not use the dual-flash loader with the current board state. Runtime
  diagnostics show only one flash path responds correctly.
- If your SEGGER install path is not:
  `C:/Program Files/SEGGER/JLink_V930a/JLinkGDBServerCL.exe`
  update `.vscode/launch.json`.
