# FarmAuto STC8H station firmware

This PlatformIO project is the first hardware-targeted station scaffold.

It currently does five things:

- builds against `Stc8hBase`;
- builds the shared FarmAuto RS485 protocol and local action controller;
- keeps a Timer0 1ms local time base for station action timing;
- parses protocol frames from UART1 byte stream;
- handles PING, motor config, start action, stop action, clear fault, and status responses.

It does not yet bind RS485 direction, AT8236 pins, PWM, Hall AB encoder inputs, INA240 ADC channel, DIP address pins, or RUN/ERR LEDs.

`Stc8hBase` already provides `drv_rs485_uart`, but that driver requires board-level `BOARD_RS485_TX_ENABLE()` and `BOARD_RS485_RX_ENABLE()` macros. FarmAuto does not enable that driver until the DE/RE pin is confirmed by the PCB.

## Local dependency

The current development checkout expects:

- `../../../../codex_dir/Stc8hBase`

Build:

```sh
pio run
```

Host smoke:

```sh
make -C ../../tools/station_node_smoke test
```
