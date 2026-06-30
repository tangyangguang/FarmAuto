# Master Services

This directory contains hardware-independent business services for the ESP32 master.

Current service:

- `fa_feed_service`: converts feeder business input into generic motor config/action requests and action results.

Out of scope for this layer:

- UART transport.
- Web request handlers.
- Persistent records.
- Scheduler decisions.
- Station scan/retry loops.
