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

## MQTT interface

MQTT is the standard Tasmota client and command dispatcher over plain TCP.
Configure MqttHost, MqttPort, MqttUser/MqttPassword if required, and SetOption3 1.
This build does not include MQTT TLS, rules, or Home Assistant discovery.
Manual MQTT light configuration can use the normal topics.

With default FullTopic and Topic `tasmota_ABAE7D`:

| Publish topic | Payload | Effect |
| --- | --- | --- |
| `cmnd/tasmota_ABAE7D/Power` | `ON`, `OFF`, `TOGGLE` | Selected group power |
| `cmnd/tasmota_ABAE7D/Dimmer` | `0..100` | Active group's brightness |
| `cmnd/tasmota_ABAE7D/CT` | `153..370` | Daylight CCT in mired |
| `cmnd/tasmota_ABAE7D/LampNight` | `0` or `1` | Select daylight/night, retain power |
| `cmnd/tasmota_ABAE7D/State` | empty | Read current light/network state |
| `cmnd/tasmota_ABAE7D/LampStatus` | empty | Group dimmers and PWM diagnostics |
| `cmnd/tasmota_ABAE7D/Backlog` | `LampNight 1; Dimmer 7; Power ON` | Ordered commands |

Night CT has no effect on the dedicated night LED. LampNight restores the
chosen group's independent dimmer. Standard SetOption20 controls whether
Dimmer/CT commands turn on an off light; LampNight selection itself does not.

Command replies normally use `stat/tasmota_ABAE7D/RESULT`; power also has its
standard POWER response. `tele/tasmota_ABAE7D/STATE` carries Power, Dimmer, CT
and LampNight. SetOption59 1 publishes STATE on light/power changes, including
LampNight changes. Periodic telemetry follows TelePeriod; availability uses
`tele/tasmota_ABAE7D/LWT`.

Use non-retained command messages for normal control. Retained commands replay
on reconnect and can immediately override the local boot decision.
