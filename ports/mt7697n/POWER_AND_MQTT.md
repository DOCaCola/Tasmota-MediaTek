# Light startup and MQTT

The native port uses Tasmota's existing `PowerOnState`, `SetOption0` and
`SaveData` settings. The light driver does not replace the common startup
policy. `PowerOnState 1` means on after cold power restoration; with
`SetOption0 1`, ordinary software restarts/OTA restore the prior power state.
`PowerOnState 3` restores saved power after both kinds of startup.

Brightness, CT, LampNight and separate day/night dimmers use Tasmota settings.
SetOption0 controls power memory only. SaveData controls periodic saving;
fades can postpone the write. A software restart saves through the normal
Tasmota restart path.

## MT7697 reset classification

The SDK whole-chip reset can clear the watchdog cause before application
startup. A 16-byte NOLOAD region at SRAM address 0x2003FFF0 holds a versioned
session/restart marker with complementary words. The linker excludes it from
the stack and startup clearing. There are no added flash or eFuse writes.

- No valid retained session: power-on.
- Retained requested-restart marker: software restart; immediately consumed.
- Retained running session without a watchdog cause: unknown warm reset.
- Hardware watchdog/software cause takes precedence.

Application/SDK emulation validates startup memory handling and policy, but
hardware warm-restart and fully discharged cold-boot validation are also required.
The first boot of this feature initializes the marker; it cannot infer the
previous firmware's restart intent.

The reserved-SRAM build has passed a hardware software-restart test with
PowerOnState 1 / SetOption0 1 and saved power OFF: it reported Software reset and
remained off. Fully discharged cold-boot validation is still pending.

## MQTT interface

MQTT is the standard Tasmota client and command dispatcher over plain TCP.
Configure MqttHost, MqttPort, MqttUser/MqttPassword if required, and SetOption3 1.
This build does not include MQTT TLS, rules, or Home Assistant discovery.
Manual MQTT light configuration can use the normal topics.

With default FullTopic and Topic `ylxd01yl`:

| Publish topic | Payload | Effect |
| --- | --- | --- |
| `cmnd/ylxd01yl/Power` | `ON`, `OFF`, `TOGGLE` | Selected group power |
| `cmnd/ylxd01yl/Dimmer` | `0..100` | Active group's brightness |
| `cmnd/ylxd01yl/CT` | `153..370` | Daylight CCT in mired |
| `cmnd/ylxd01yl/LampNight` | `0` or `1` | Select daylight/night, retain power |
| `cmnd/ylxd01yl/State` | empty | Read current light/network state |
| `cmnd/ylxd01yl/LampStatus` | empty | Group dimmers and PWM diagnostics |
| `cmnd/ylxd01yl/Backlog` | `LampNight 1; Dimmer 7; Power ON` | Ordered commands |

Night CT has no effect on the dedicated night LED. LampNight restores the
chosen group's independent dimmer. Standard SetOption20 controls whether
Dimmer/CT commands turn on an off light; LampNight selection itself does not.

Command replies normally use `stat/ylxd01yl/RESULT`; power also has its
standard POWER response. `tele/ylxd01yl/STATE` carries Power, Dimmer, CT
and LampNight. SetOption59 1 publishes STATE on light/power changes, including
LampNight changes. Periodic telemetry follows TelePeriod; availability uses
`tele/ylxd01yl/LWT`.

Use non-retained command messages for normal control. Retained commands replay
on reconnect and can immediately override the local boot decision.

Hardware tests with a temporary local broker verified these controls, state and
availability messages, separate dimmer restoration, SetOption20, and night PWM
readback. Original broker and light settings were restored after the test.
