# Phase 1 engineering report

Date: 2026-11-01. Status: software implementation complete; target build in
progress; sensor acceptance **not met**. Hardware execution is blocked on
verified splice and power, not by evidence of a defective sensor.

## Confirmed

- Repository inspection found only initial README on main and scaffold refs,
  both at cd9e669. No AGENTS.md or existing tests. Work is isolated on
  `feat/phase1-c5-bringup`; main is unchanged.
- Original SF04 examples, LG16 datasheet, Sensirion implementation guide,
  LilyGO schematic/board files and version-specific IDF sources were inspected.
- USB VID:PID 303A:1001, serial 10:BD:A3:EC:1C:70, port
  `/dev/cu.usbmodem1101` responds to ROM queries as ESP32-C5HR8 revision 1.2,
  48 MHz crystal, embedded 8 MB PSRAM. Flash manufacturer EF, device 4018,
  capacity 16 MB. Queries used no-reset / no-stub / no flash writes.
- The other USB serial port, `/dev/cu.usbmodemSN234567892`, identifies as a
  USB BillBoard (310E:B016), not an ESP ROM target. It was not opened.
- LilyGO V1.1 schematic P3 exposes GND, 3.3 V, U0TXD/GPIO11 and U0RXD/GPIO12.
  The connector is not a 5 V sensor power source. GPIO matrix can route I²C,
  but SDA/SCL at the physical splice remain unverified.
- Host C11 tests pass with strict warnings / Werror and active assertions.
  Coverage includes the published SF04 CRC vector, multiword CRC atomicity,
  signed extremes, positive/negative LG16 CF0 conversion, all field selectors,
  invalid calibration, truncation, NACK, timeout, bus error, metadata addressing
  and identity parsing. See `evidence/host-tests.txt`.

## Implemented diagnostic behavior

Default build refuses I²C initialization until physical checks are recorded in
configuration. Enabled diagnostic initializes at 100 kHz, probes only 0x40 once,
then reads documented product identity and active calibration. Every word CRC
is checked; failed transport/CRC/invalid values produce explicit statuses.
There is no fallback scale, acquisition loop, configuration write, EEPROM data
write, reset or arbitrary recovery command. Runtime timestamps are monotonic
milliseconds from boot; firmware prints revision, IDF version and ELF SHA256.

Protocol tests use a fake transport. They establish software behavior only,
not sensor communication or electrical timing. IDF's fixed-length receive API
cannot expose a partial byte count, so failed transactions never enter decoding.
Raw user register is available after valid metadata; status interpretations
beyond the documented field selector are deliberately absent.

## Unresolved hardware and next action

The supplied sensor identity is LG16-2000D, label 2205-00258; neither has been
read electronically. Active field, scale factor and units are unknown. The
published standard scale 5 is only a unit-test fixture. Extended-range support
from serial chronology does not establish the currently selected field.

User confirmed intended I²C use and prior success with a Teensy setup, but did
not provide measured supply or splice continuity. A direct P3 power connection
would supply 3.3 V, below the LG16 minimum 4.0 V. Whether an external 5 V supply
exists, whether its power is isolated from the 3.3 V rail, and whether SDA/SCL
pull up to 3.3 V are unknown. This is a specific unresolved electrical mismatch,
not a proven short or proof of sensor damage. Do not energize an uncertain splice.

Physical dongle model/revision must be confirmed against its marking; the ROM
identity proves C5 silicon, not the complete board model. Existing bootloader
and partition compatibility have not been read or changed.

No firmware flash, application startup, I²C ACK, metadata or 30-second diagnostic
capture is claimed. Complete the continuity and voltage checks in `bringup.md`,
verify app/bootloader/partition compatibility, then flash only the verified app
region and capture at least 30 seconds. A measured NACK/timeout with accurate
logs is a useful hardware diagnostic result, but acceptance requires ACK at
0x40; metadata success is the next level.
