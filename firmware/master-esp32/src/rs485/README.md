# Master RS485 Core

This directory contains the hardware-independent part of the ESP32 master's RS485 application protocol.

Current scope:

- Build FarmAuto request frames.
- Maintain request sequence numbers.
- Parse common responses, `PING`, and `GET_STATUS`.

Out of scope:

- UART driver and RS485 DE/RE timing.
- Address scan scheduling and retry policy.
- Device registry, Web API, records, and automatic plans.

The ESP32 firmware will wrap this core with the actual serial transport provided by the board and `Esp32Base`.
