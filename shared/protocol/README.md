# FarmAuto Shared Protocol

This directory contains the small C protocol layer shared by the ESP32 master, STC8H station, and host-side smoke tests.

Scope:

- RS485 frame constants and command/status enums.
- CRC16/MODBUS.
- Fixed-size frame encode/decode.
- UART byte-stream frame parser.
- Little-endian payload reader/writer helpers.

Out of scope:

- Web APIs.
- Device registry, schedules, and records.
- Motor PWM, encoder sampling, current sensing, and action state machines.

Run the host smoke test:

```sh
make -C tools/protocol_smoke test
```
