# Esp32EncodedDcMotor

Non-blocking encoded DC motor control skeleton.

Current status:

- Defines public state, configuration, snapshot, trace, driver, and encoder interfaces.
- Provides a minimal single-motor state machine with independent soft-start and soft-stop fields.
- Does not yet include ESP32 PCNT, ESP32Encoder, LEDC, or AT8236 hardware adapters.
- Does not implement PID, S-curve motion, or multi-motor orchestration.

The core library does not depend on Esp32Base. Applications decide Web/API behavior, event logging, persistent configuration, and multi-actuator scheduling.
