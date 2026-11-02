# LC Flow Monitor

Phase 1 native C diagnostic for LilyGO T-Dongle C5 / Sensirion LG16-2000D.
**Hardware acceptance is pending:** no sensor ACK or metadata capture yet.

The default firmware builds but leaves I²C disabled until the splice, supply,
and GPIO assignment are physically verified. The board connector supplies 3.3 V;
the LG16 requires 4–12 V (nominal 5 V). Connector compatibility alone does not
establish electrical compatibility. See [bring-up instructions](docs/bringup.md)
and the [engineering report](docs/report.md).

## Build

Use ESP-IDF **v5.5.1**, commit `fcae32885b0296b32044cb99ecbdc50d98dddb83`.
Keep this installation separate from any existing IDF installation.

```sh
git clone --branch v5.5.1 --depth 1 --recursive https://github.com/espressif/esp-idf.git /path/to/esp-idf-v5.5.1
/path/to/esp-idf-v5.5.1/install.sh esp32c5
. /path/to/esp-idf-v5.5.1/export.sh
idf.py set-target esp32c5
idf.py build
```

Flash configuration: QIO, 80 MHz, 16 MB; USB Serial/JTAG console; no PSRAM.
`idf.py menuconfig` → **LC Flow Monitor board verification** holds the explicit
hardware gate and SDA/SCL values. Defaults are unverified / `-1` / `-1`.
Do not enable them based only on cable color or connector shape.

## Host tests

```sh
cmake -S tests -B build-host -DCMAKE_BUILD_TYPE=Release
cmake --build build-host
ctest --test-dir build-host --output-on-failure
```

Tests use C11, strict warnings and `-Werror`, with assertions active in Release.
The C protocol library is independent of ESP-IDF and board pins. Diagnostics
perform one address probe, then CRC-checked identity/calibration reads if the
sensor responds. No EEPROM data, configuration writes, reset or acquisition
commands are implemented. Timestamped records use milliseconds since boot.

`components/sf04/` contains the driver; `main/board.h` contains board policy;
`main/main.c` supplies the ESP-IDF transport. Discovery/configuration and numeric
conversion are separate. The published CF0 scale (5 counts per µl/min) is only
a test fixture, never a substitute for a failed hardware read.

See [references](docs/references.md) for protocol operations and source revisions.
