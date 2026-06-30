# FarmAuto ESP32 master firmware

This PlatformIO project is the first hardware-targeted master scaffold.

It intentionally does only three things now:

- builds against `Esp32Base`;
- builds the shared FarmAuto RS485 protocol and master/feed-service core;
- starts the Esp32Base runtime loop.

It does not yet bind RS485 UART pins, buttons, LEDs, RTC, FRAM, SHT30, or business Web pages.

## Local dependency

The current development checkout expects:

- `../../../Esp32Base`

Build:

```sh
pio run
```
