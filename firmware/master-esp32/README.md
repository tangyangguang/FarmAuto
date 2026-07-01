# FarmAuto ESP32 master firmware

This PlatformIO project is the first hardware-targeted master scaffold.

It currently does these things:

- builds against `Esp32Base`;
- builds the shared FarmAuto RS485 protocol and master feed/door/action-record core;
- explicitly compiles shared protocol sources into the ESP32 target;
- initializes a LittleFS-backed action record ring via `Esp32BaseFs`;
- registers FarmAuto feeder, door, automatic schedule and RS485 parameters through `Esp32BaseAppConfig`;
- exposes `/feed`, `/door`, `/auto`, `/records`, `/devices`, `/notify` and `/bus` pages;
- sends bounded manual and scheduled actions over RS485 when transport pins are configured;
- previews manual feed/door actions when RS485 is not configured;
- tracks active actions to terminal state and writes action records;
- stores device names, display order, station bindings and station enabled state in LittleFS;
- scans RS485 addresses `1..127` and records discovered stations;
- starts the Esp32Base runtime loop.

It does not yet bind buttons, LEDs, RTC, FRAM, SHT30, Bafa/WeChat sending, station OTA, missing-feed detection or final PCB pin-specific board IO.

The current action record file is `/farmauto/action-records.bin`. It is a small FarmAuto business ring file built on `Esp32BaseFs`; it is not the FRAM pending-action journal.

Automatic schedules require real time to be synced. Missed schedule points are not backfilled; blocked runs are logged and skipped.

## Local dependency

The current development checkout expects:

- `../../../Esp32Base`

Build:

```sh
pio run
```
