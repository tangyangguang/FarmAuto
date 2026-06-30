# Master Services

This directory contains hardware-independent business services for the ESP32 master.

Current service:

- `fa_feed_service`: converts feeder business input into generic motor config/action requests and action results.
- `fa_action_record`: keeps the minimal in-memory action record from start parameters and station status.

Out of scope for this layer:

- UART transport.
- Web request handlers.
- LittleFS/FRAM writes. The ESP32-specific LittleFS action record ring lives in `../storage`.
- Scheduler decisions.
- Station scan/retry loops.
