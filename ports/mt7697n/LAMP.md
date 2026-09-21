# YLXD01YL light driver

The native build enables Tasmota's CCT engine and XLGT12, a board-specific
output driver. Generic GPIO/PWM reassignment remains disabled.

- `Power`, `Dimmer`, `CT`, `Fade`, `Speed` use standard Tasmota behavior.
- Physical temperature range is 2700–6500 K (CT 153–370, endpoint clamped).
- `LampNight 0` selects daylight; `LampNight 1` selects night mode.
  Query `LampNight` for mode and `LampReady`. The web root has both mode
  buttons alongside the normal light controls. Mode changes do not turn
  power on; use the normal power button. Brightness is shared between modes.
- Mode is stored in the native settings block (schema at 0x404, mode at
  0x405); total settings length and following offsets are unchanged.
- The first lighting-capable boot sets power off, PowerOnState 0, daylight,
  and Dimmer 10. Later boots respect the configured power-on policy.
- SetOption37/68/92 cannot enable channel remapping, independent dimmers or
  brightness/CT signal mode: those do not represent this board.

Cold/warm frames use the recovered eleven-point stock calibration table
with bounded piecewise-linear interpolation. Tasmota applies its brightness
curve once; this is not an exact reproduction of stock interpolation or its
minimum-brightness curve. Intermediate fade frames determine temperature.
Combined daylight duty never exceeds 4000/4000; night and daylight never
overlap. These software limits do not replace electrical verification.

Raw GPIO31/PWM32 is warm, GPIO32/PWM33 cold, GPIO30/PWM31 night, mux9,
40 MHz source and 10 kHz period. Initialization writes zero before pinmux.
Partial startup/update failure stops all three outputs. Restart requests
turn outputs off before reboot. HAL errors are logged and `LampReady` becomes
false; further output is disabled until reboot.

Tests run through `tests/run.py`: calibration anchors, full-domain bounds
and monotonicity, actual driver dispatch with simulated HAL, mode handover,
settings migration, web commands, restart, and HAL failure. Verify real
warm/cold identity, waveform and low-level night output before treating
the image as a completed hardware port.
