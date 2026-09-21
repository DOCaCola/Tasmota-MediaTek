# YLXD01YL light driver

The native build enables Tasmota's CCT engine and XLGT12, a board-specific
output driver. Generic GPIO/PWM reassignment remains disabled.

**Investigation correction:** the stock product's transition controller sets
minimum brightness15 before daylight conversion (night0). This driver currently
uses0. The exhaustive comparison described below validates that isolated
converter configuration, not the complete stock request-to-output path. See
`RnD/stock-fade-investigation.md` in the workspace. Duty-space stock fades and
separate day/night brightness retention are also not implemented.

- `Power`, `Dimmer`, `CT`, `Fade`, `Speed` use standard Tasmota behavior.
- Physical temperature range is 2700–6500 K (CT 153–370, endpoint clamped).
- `LampNight 0` selects daylight; `LampNight 1` selects night mode.
  Query `LampNight` for mode and `LampReady`. The web root has both mode
  buttons alongside the normal light controls. Mode changes do not turn
  power on; use the normal power button. Brightness is shared between modes.
- Mode is stored in the native settings block (schema at 0x404, mode at
  0x405); total settings length and following offsets are unchanged.
- The first lighting-capable boot sets power off, PowerOnState 0, daylight,
  and Dimmer 10. Native cold-boot reset classification is still incomplete:
  unknown resets currently take Tasmota's saved-state restart path. Do not
  assume PowerOnState 0 is enforced on every subsequent power cycle.
- SetOption37/68/92 cannot enable channel remapping, independent dimmers or
  brightness/CT signal mode: those do not represent this board.

Cold/warm frames use the stock eleven-point table, three-point Lagrange
interpolation, endpoint plateaus, and per-channel minimum-duty correction.
For a nonzero channel, duty is `4000*(0.08+0.92*coefficient*brightness)`;
zero remains off. Coefficient and brightness are fractions. The implementation
preserves the stock floating-point evaluation and rounding. Cortex-M4 and host
results match all 383901 integer Kelvin/percent combinations against execution
of stock 1.5.9_0189 ARM instructions. Night output is linear, without that floor.

Schema 2 migration disables LedTable to use linear requested brightness,
matching stock's default. Users may explicitly enable gamma again. Tasmota's
10-bit frame quantization and mired representation remain; comparison of the
converter itself does not imply identical UI-to-output behavior.
Tasmota still owns Fade/Speed and intermediate virtual channel frames; its
transition timing/curve is not a clone of stock's duty-space transition engine.
See `RnD/stock-light-algorithm.md` in the investigation workspace.
Combined daylight duty never exceeds 4000/4000; night and daylight never
overlap. These software limits do not replace electrical verification.

Raw GPIO31/PWM32 is warm, GPIO32/PWM33 cold, GPIO30/PWM31 night, mux9,
40 MHz source and 10 kHz period. Initialization writes zero before pinmux.
Partial startup/update failure stops all three outputs. Restart requests
turn outputs off before reboot. HAL errors are logged and `LampReady` becomes
false; further output is disabled until reboot.
Duty updates check hardware running status and restart idle channels, as stock
does. `LampStatus` reads back duty, frequency and running flags in warm/cold/night
order, alongside the requested target and cold/warm virtual frame. Readback
describes PWM hardware; it does not measure emitted light.

Tests run through `tests/run.py`: calibration anchors, full-domain bounds
and monotonicity, actual driver dispatch with simulated HAL, mode handover,
settings migration, web commands, restart, and HAL failure. Verify real
warm/cold identity, waveform and low-level night output before treating
the image as a completed hardware port.
