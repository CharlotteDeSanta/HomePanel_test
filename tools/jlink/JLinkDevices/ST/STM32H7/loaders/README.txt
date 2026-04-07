Place the SEGGER single-flash QSPI loader in this directory with this exact file name:

  ST_STM32H753I_EVAL_QSPI.elf

Recommended source:
  https://kb.segger.com/STM32H753I-EVAL

Use the "ST STM32H753I EVAL QSPI.elf" file from that page.
After downloading it, rename it to the file name above and place it next to
this README.txt file.

Why this loader:
  It matches the working BK0 QSPI pin layout used by this project:
    CLK  = PB2
    CS   = PG6
    BK0  = PF8 / PF9 / PF7 / PF6

Why not the dual-flash loader:
  Runtime diagnostics show only one flash path is readable on this board.
  Using the dual-flash loader programs resources in a layout that does not
  match what the firmware can read here.
