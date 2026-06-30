# FarmAuto ESP32 master firmware

This PlatformIO project is the first hardware-targeted master scaffold.

It currently does these things:

- builds against `Esp32Base`;
- builds the shared FarmAuto RS485 protocol and master/feed-service/action-record core;
- explicitly compiles shared protocol sources into the ESP32 target;
- initializes a LittleFS-backed action record ring via `Esp32BaseFs`;
- registers FarmAuto feeder parameters through `Esp32BaseAppConfig`;
- exposes a minimal `/feed` page and `/api/feed/manual` dry-run action builder;
- starts the Esp32Base runtime loop.

It does not yet bind RS485 UART pins, buttons, LEDs, RTC, FRAM, SHT30, or the full V3 business Web pages.

The current action record file is `/farmauto/action-records.bin`. It is a small FarmAuto business ring file built on `Esp32BaseFs`; it is not the FRAM pending-action journal.

The current Web manual feed endpoint is intentionally a dry run. It builds the bounded motor action from saved config and returns the action preview, but does not send RS485 and does not write a completed action record.

## Local dependency

The current development checkout expects:

- `../../../Esp32Base`

Build:

```sh
pio run
```
