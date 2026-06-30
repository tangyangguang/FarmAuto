# Master RS485

This directory contains the ESP32 master's FarmAuto RS485 protocol code.

Current hardware-independent core scope:

- Build FarmAuto request frames.
- Maintain request sequence numbers.
- Parse common responses, `PING`, and `GET_STATUS`.
- Build `STOP_ACTION` for immediate station stop requests.

Current ESP32 transport scope:

- Use configurable ESP32 `HardwareSerial` UART 1 or 2.
- Use one configurable DE pin for half-duplex RS485 direction control.
- Default bus rate is `115200 8N1`.
- Default request timeout is `80 ms`.
- RX/TX/DE pins default to `-1`, so transport is disabled until PCB pins are configured through Esp32Base App Config.
- A transaction writes one complete protocol frame and waits for one complete response frame.

Out of scope:

- Address scan scheduling and retry policy.
- Device registry, Web API, records, and automatic plans.

The current Web manual feed endpoint uses this transport only when it is configured and ready. Otherwise it stays in dry-run mode and only builds the action preview.
