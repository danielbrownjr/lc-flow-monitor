# Hardware bring-up procedure

## Current board configuration

Board reference is LilyGO T-Dongle C5, ESP32-C5HR8, native USB D−/D+ on
GPIO13/14. USB console avoids driving the connector as UART. TFT, LEDs, SD,
radios and PSRAM are not initialized by this application.

The V1.1 schematic P3 connector, read visually and checked against `pin_config.h`:

| P3 contact | Net | MCU / function |
| --- | --- | --- |
| 1 | GND | Common ground |
| 2 | 3.3V | Regulated supply, **not 5 V** |
| 3 | U0TXD | GPIO11; may be routed as I²C |
| 4 | U0RXD | GPIO12; may be routed as I²C |
| 5, 6 | GND | Connector mechanical/shield contacts |

A conventional Qwiic pin assignment would put SDA on contact 3 / GPIO11 and
SCL on contact 4 / GPIO12. This is a candidate, not a verified splice. Neither
the schematic nor the sample UART pin names prove which sensor wire reaches
which contact. The V1.0 pin diagram and V1.1 schematic agree on these signals;
verify the actual PCB revision. GPIO11/12 support matrix routing for I²C.

Sensor connector per LG16 datasheet section 2.4: 1 SDA, 2 SCL, 3 VDD,
4 GND, 5 unconnected. Pin numbering must be checked against the connector
orientation in the datasheet, not inferred from the wire colors.

## Electrical checks before enabling diagnostics

Power down and inspect continuity through the entire spliced cable, including
shorts between adjacent conductors. Record board revision, sensor label
`2205-00258`, and which MCU pin reaches each sensor contact.

The sensor requires 4.0–12.0 V; nominal 5 V is the intended bring-up supply.
P3 contact 2 is only 3.3 V and cannot meet that requirement. If supplying the
sensor separately with regulated 5 V, isolate the connector's 3.3 V power wire
from sensor VDD; do not connect 5 V back into the board's 3.3 V rail. Share GND.
Verify actual voltage **at sensor VDD/GND**, under load. Firmware cannot infer
that external supply voltage from connector shape or USB enumeration.

SDA/SCL must have external pull-ups to **3.3 V**, with idle levels measured at
both ends. The sensor accepts 3.3 V I²C while powered at 5 V. Inspect any adapter
for pull-ups to 5 V before attaching ESP32 GPIO. Internal pull-ups are disabled;
select external values for actual cable capacitance (typically 2–5 kΩ per IDF).
No on-board SDA/SCL pull-up is shown on P3. Do not energize an uncertain splice.

Only after those measurements, set `LC_HARDWARE_VERIFIED=y` and explicitly set
SDA/SCL to the verified assignment (11/12 or 12/11). Other assignments remain
blocked by board policy. Rebuild; save sdkconfig and a physical wiring record
with the test evidence. Supply, wiring and idle-level verification are currently
missing, so the checked-in defaults perform no GPIO or sensor operations.

## Identify, build and flash

1. Inventory with `python -m serial.tools.list_ports -v`. Confirm physical dongle,
   USB location, VID/PID and serial. Other serial devices must remain untouched.
2. Read ROM identity and flash ID on that port using `esptool --port PORT
   --before no-reset --after no-reset --no-stub chip-id` and `flash-id`.
   A C5 chip ID alone does not establish board model; compare its physical label.
3. Export the pinned IDF, run `idf.py set-target esp32c5` and `idf.py build`,
   capturing stdout/stderr and exit code. Check QIO / 80 MHz / 16 MB in sdkconfig.
4. Before a future flash, read/back up the existing partition table, bootloader
   and affected app region; confirm the existing bootloader accepts this IDF
   application and determine the actual app partition offset and size. Do not
   assume that an Arduino factory layout matches IDF's default single-app layout.
   A verified compatible app partition permits `idf.py -p PORT app-flash` using
   matching partition configuration. Review generated flash arguments first.
   Do not run full `flash`, `erase-flash`, or replace bootloader/table by default.
   If compatibility is unresolved, stop and document it before writing flash.
5. If download mode is needed, LilyGO documents holding BOOT while inserting
   the dongle; release after enumeration. Re-inventory because the port may change.
   Current ROM reads succeeded without requesting a reset.
6. Capture startup and at least 30 seconds after the verified app flash. Install
   pyserial in the IDF environment if needed. `python tools/capture_serial.py
   PORT docs/evidence/serial.txt --seconds 35` only reads, with RTS/DTR disabled.
   Start capture before a controlled device restart to include startup identity.
   Check firmware revision/ELF SHA, I²C init, address ACK and all metadata statuses.

No flash command has been executed in this session. The ROM was left in download
mode by the explicitly no-reset/no-stub queries; it has not run our application.

## Diagnostic interpretation

One address-only probe occurs at 100 kHz after 200 ms startup delay. ACK permits
read-only product and calibration operations. NACK/timeout/bus errors stop further
sensor transactions; there is no scan or automatic retry. Metadata CRC failures
are explicit and never converted into valid measurements. Completion records
continue every five seconds without further I²C operations. Unverified builds
print BLOCKED and return before initializing I²C.

| Observation | Hypotheses / next evidence |
| --- | --- |
| NACK at 0x40 | Check sensor supply first, common ground, SDA/SCL continuity, connector reversal, and actual address; address can be configured externally. |
| Timeout | Check SDA/SCL stuck low, shorts, pull-ups, clock stretching and rise time with scope/logic analyzer. IDF may group arbitration faults as timeout. |
| ACK then command NACK | Could be pending measurement state from earlier firmware or wrong protocol; do not send arbitrary reset/recovery commands. Power-cycle only after electrical checks. |
| CRC error | Capture bus waveform; investigate cable capacitance, ringing, rise time, noise or incorrect transaction framing. |
| Invalid calibration | Preserve raw capture and inspect documented fields; do not select CF0 or hard-code 5 as recovery. |
| BUS_ERROR | Inspect raw IDF error first; allocation, controller state and API setup errors are not proof of sensor failure. |

The IDF legacy transaction API reports ACK failure as ESP_FAIL and busy/timeout
as ESP_ERR_TIMEOUT. Other errors retain their IDF names as BUS_ERROR. It does not
return a partial receive count; partial transactions are rejected before decoding.
Host tests independently exercise truncated buffers. No dedicated electrical bus
error register or undocumented sensor status bits are interpreted.
