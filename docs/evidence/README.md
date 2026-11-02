# Evidence index

All records were captured locally on 2026-11-01.

- `usb-inventory.txt`: pyserial USB enumeration, before any firmware flash.
- `c5-rom-flash-id.txt`: esptool 5.6.0 no-reset/no-stub ROM identity and flash-ID read.
- `host-tests.txt`: strict host build, CTest and test coverage summary; exit 0.
- `target-config.txt`: successful pinned IDF `set-target esp32c5` output.
- `build-default.txt`: successful final build command output after the scoped
  vendor-header warning fix. Dependencies compiled on the initial attempt were
  reused; the enabled build below recompiles independently from a fresh directory.
- `build-enabled-compile-only.txt`: fresh full build with synthetic enabled GPIO
  configuration, used only to compile/link the diagnostic path; exit 0.
- `image-info-*.txt`: esptool offline image validation and embedded app/IDF revision.
- `build-manifest.txt`: source revision, commands, configuration, binary/ELF hashes.
- `reference-sha256.txt`: downloaded reference hashes; URLs are in `../references.md`.

IDF's successful build output prints suggested full-flash commands. **Those
suggestions were not executed.** Do not copy them without the compatibility and
electrical checks in `../bringup.md`. There is no flash log or sensor serial
capture because neither firmware image was written to the device. Default build
keeps I²C disabled. The compile-only enabled image must not be treated as a
hardware-approved image.
