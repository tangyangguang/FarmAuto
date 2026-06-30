# Station Action Controller

This module is the hardware-independent motor action state machine for the STC8H station.

It does not touch GPIO, PWM, ADC, encoder interrupts, or RS485 directly. The station app feeds it:

- Current time in milliseconds.
- Current encoder position in pulses.
- Filtered current in mA.
- A validated action request from the RS485 protocol layer.

It returns:

- Whether the motor should be enabled.
- Direction and speed.
- Brake request.
- Motor state, stop reason, completed pulses, and fault code.

The real station firmware will connect this module to AT8236, Hall AB encoder, INA240A2 current sensing, and RS485 command handling through board-specific adapters.
