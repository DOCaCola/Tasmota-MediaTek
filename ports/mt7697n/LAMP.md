# YLXD01YL light driver

The native build uses Tasmota's CCT controls and XLGT12 with the stock lamp's
calibrated PWM targets and ordinary setting-transition algorithm.

- `Power`, `Dimmer`, `CT` control the selected day/night group.
- `LampNight 0/1` selects day/night without changing power. Each group remembers
  its own dimmer. Schema 3 preserves the selected group's existing dimmer;
  the other defaults to day 10% or night 5%. Off retains brightness.
- `Fade 0` applies targets immediately. `Fade 1` uses calibrated duty-space
  transitions. `Speed` sets duration to speed * 500 ms; scheduler interval 10 ms.
  New settings replace the current transition from its last emitted PWM duty.
  Day is linear; night uses the stock directional cubic when brightness changes.
- Day CT range is 153–370 mired, clamped to 6500–2700 K at the endpoints.
- `LedTable` stays off: the calibrated driver owns the brightness transfer function.
- `LampStatus` returns mode, separate dimmers, fade activity, target duties and
  hardware duty/frequency/running readback in warm/cold/night order.

Day conversion uses stock's minimum-brightness parameter 15: requested 1–100%
becomes 15–100% before the calibrated eleven-point CT table and 8% electrical
floor are applied. Zero remains off. Night has linear 0–100% brightness without
these daylight floors. This corrects the earlier isolated converter comparison,
which used minimum 0 despite runtime stock setup selecting 15.

All 383901 daylight Kelvin/percent targets match original stock 1.5.9_0189 ARM
execution, for both host and Cortex-M4 builds. 1800 original ARM transition
vectors also match both builds. The implementation covers ordinary setting
replacement, not the stock flow-effect queue or its in-place flow retarget mode.
Tasmota Wakeup supplies its own evolving brightness without an extra fade.
Cross-group selection disables the previous group and starts the selected group
from off; stock cross-group scheduling has not been claimed equivalent.

Each channel is bounded to 4000 counts. Combined daylight is bounded to 4320:
stock fades between pure warm and pure cold temporarily substitute 320 for zero
endpoints. Night/day never overlap. Updates reduce duties before increasing
other channels, preserving bounds at each HAL call.

GPIO31/PWM32 warm, GPIO32/PWM33 cold and GPIO30/PWM31 night use mux9, a 40 MHz
source and 10 kHz PWM. Initialization writes zero before pinmux. Partial HAL
failure stops all outputs and disables updates until reboot. Restart turns them
off. Hardware readback does not measure emitted light.

Schema/mode/day-dimmer/night-dimmer occupy settings offsets 0x404–0x407;
settings length and subsequent fields are unchanged. First lighting-capable boot
starts off with PowerOnState 0. Native unknown-reset classification still takes
the saved-state restart path: subsequent cold-boot PowerOnState policy remains
unverified. SetOption37/68/92 cannot remap this board's fixed channel topology.

Validation: tests/run.py covers driver dispatch, migration, separate dimmers,
HAL failure, fade replacement, timer wrap and bounds; arm_runtime_test.py runs
stock-derived targets/fades on Cortex-M4 with the SDK runtime. Research and
reproduction tools are in the workspace RnD folder.
