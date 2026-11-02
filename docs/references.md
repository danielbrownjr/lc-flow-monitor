# Reference audit

Inspected before implementation on 2026-11-01:

- Local `main`, `origin/main`, `origin/feat/phase1-native-c-scaffold`: all initially
  `cd9e669` with only the same 42-byte README. No tests, starter implementation,
  AGENTS.md or inherited project instructions found. Clean worktree preserved.
- [Original reference repository](https://github.com/danielbrownjr/arduino-liquid-flow-snippets),
  commit `92e8e10042474b83678ac84e8eb20fc98c6872dd`: SF04 examples 03, 04, 05,
  10, 12 and README. Protocol studied; driver written in C rather than porting
  Arduino runtime code. The example resets/writes/acquisition loops are excluded.
- [LG16 datasheet, version 8, January 2020](https://sensirion.com/media/documents/690EAAD4/61658721/Sensirion_Liquid_Flow_Meters_LG16_xxxxD_Datasheet.pdf):
  sections 2–4 establish supply, timing, pinout, signed conversion and bus levels.
- [Sensirion SF04 implementation guide, version 1, June 2021](https://sensirion.com/resource/application_note/implementation-guide-i2c-protocol):
  sections 3.7, 5.3, 5.5, 6.1, 6.2 and 6.4 establish CRC and metadata sequences.
- [LilyGO repository](https://github.com/Xinyuan-LilyGO/T-Dongle-C5),
  master `0fe7367959509af38b45bd1b81471018932df625`: `readme.md`,
  `include/pin_config.h`, `board/Lilygo-T-Dongle-C5.json`,
  `hardware/T-Dongle-C5 V1.1.pdf`.
- [LilyGO wiki](https://wiki.lilygo.cc/products/t-dongle-series/t-dongle-c5/):
  connector pin diagram and board specifications. Diagram says V1.0; current
  repository schematic is V1.1. Physical board revision remains to be checked.
- [ESP-IDF 5.5.1 I²C documentation](https://docs.espressif.com/projects/esp-idf/en/v5.5.1/esp32c5/api-reference/peripherals/i2c.html)
  and installed v5.5.1 sources, especially `components/driver/i2c/include/driver/i2c.h`
  and `components/driver/i2c/i2c.c`. The legacy driver is intentionally selected:
  its synchronous command return distinguishes ESP_FAIL (NACK) and ESP_ERR_TIMEOUT;
  the 5.5.1 new driver's synchronous path reports ESP_ERR_INVALID_STATE for
  several failures. Other errors map to BUS_ERROR and retain the raw IDF name.
  Legacy API deprecation is a known maintenance limitation of this pinned build.

## Read-only protocol contract

| Operation | Write bytes (after 7-bit address 0x40) | Read bytes |
| --- | --- | --- |
| User register | E3 | 2 data + CRC |
| Part name | FA 2E 80 | 10 × (2 data + CRC) |
| Product serial | FA 2F 80 | 2 × (2 data + CRC) |
| Scale and unit | FA followed by left-aligned 12-bit word address | 2 × (2 data + CRC) |

Calibration field = user-register bits 6:4. Only fields 0–4 are addressable;
scale word address = `0x2B6 + field * 0x300`, unit is the adjacent word.
The guide specifies field 4 unit at `0xEB7`; the reference sketch's unused
UNIT_ADDRESSES table has an apparent typo (`0xEB6`). This driver reads adjacent
words, as documented, and tests all selectors including rejection of 5–7.

CRC-8: polynomial 0x31 (full polynomial 0x131), initial 0, no reflection or
final XOR; restarted per pair. Published guide example `0E 00 → 6D` anchors the
test independently. A three-byte FA/address command selects a read pointer and
contains no EEPROM write data. No generic raw-write public driver API exists.
Registers and EEPROM reads use command/address selection followed by a separate
read transaction, consistent with reference sketches. No retry loop is used.

Product serial is reported as the documented 32-bit integer. Its relationship
to printed label `2205-00258` is not assumed. Raw user register is reported as
status/configuration; no undocumented status meanings are invented.
