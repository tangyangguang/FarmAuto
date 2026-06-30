# FarmAuto STC8H station firmware

This PlatformIO project is the first hardware-targeted station scaffold.

It intentionally does only three things now:

- builds against `Stc8hBase`;
- builds the shared FarmAuto RS485 protocol and local action controller;
- starts a minimal UART heartbeat for board bring-up.

It does not yet bind RS485 direction, AT8236 pins, PWM, Hall AB encoder inputs, INA240 ADC channel, DIP address pins, or RUN/ERR LEDs.

## Local dependency

The current development checkout expects:

- `../../../../codex_dir/Stc8hBase`

Build:

```sh
pio run
```
