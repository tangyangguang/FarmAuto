# FarmAuto ESP32 master firmware

This PlatformIO project is the first hardware-targeted master scaffold.

It currently does these things:

- builds against `Esp32Base`;
- builds the shared FarmAuto RS485 protocol and master/feed-service/action-record core;
- explicitly compiles shared protocol sources into the ESP32 target;
- initializes a LittleFS-backed action record ring via `Esp32BaseFs`;
- starts the Esp32Base runtime loop.

It does not yet bind RS485 UART pins, buttons, LEDs, RTC, FRAM, SHT30, or business Web pages.

The current action record file is `/farmauto/action-records.bin`. It is a small FarmAuto business ring file built on `Esp32BaseFs`; it is not the FRAM pending-action journal.

## Local dependency

The current development checkout expects:

- `../../../Esp32Base`

Build:

```sh
pio run
```
