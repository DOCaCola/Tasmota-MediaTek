# Tasmota-MediaTek

Experimental native **MediaTek MT7697N** port of [Tasmota](https://github.com/arendst/Tasmota).
The currently tested device is the **Yeelight / Yeelink YLXD01YL ceiling lamp**
with 4 MiB GD25Q32 flash. Other MediaTek chips and boards are not validated.

This is an independent development fork, not an official Tasmota target.
It is a source repository: **no releases, prebuilt firmware distribution, or
automatic deployment** are provided. Upstream ESP8266/ESP32 binaries and the
Tasmota WebInstaller do not install this port.

## Current status

Demonstrated on the YLXD01YL: Wi-Fi setup AP and browser configuration, station
networking, HTTP/web console, persistent settings, MQTT control/state reporting,
HTTP application OTA, calibrated daylight CCT and separate nightlight PWM,
and one locally paired BLE handheld remote (PID 0x0153).

This is not a claim of production readiness. Fully discharged cold-boot state
validation, exhaustive remote mappings, broader hardware support, and long-term
network reliability remain open. Historical intermittent Wi-Fi/DHCP and HTTP
failures are not all explained. MQTT TLS, WPA3/SAE, rules, Home Assistant discovery,
generic GPIO templates, and web file upload/settings restore are not included.

## Build and development

Start with the [MT7697N build guide](ports/mt7697n/README.md).
The full application build is currently a **Windows x64** workflow using Python
3.12+, pinned GCC 13, and the vendor SDK C/C++ runtime. It does not use PlatformIO.
The portable host tests have a separate CI workflow; CI never publishes firmware.

- [Installation and OTA constraints](ports/mt7697n/INSTALLATION.md)
- [Light controls and PWM behavior](ports/mt7697n/LAMP.md)
- [MQTT and power-on settings](ports/mt7697n/POWER_AND_MQTT.md)
- [BLE handheld remote](ports/mt7697n/REMOTE.md)
- [Optional diagnostics](ports/mt7697n/DIAGNOSTICS.md)
- [Contributing](CONTRIBUTING.md) and [security scope](SECURITY.md)
- [Third-party provenance and open licensing questions](ports/mt7697n/THIRD_PARTY.md)

The initial serial conversion from stock requires a compatible bootloader,
radio image, and complete SDK flash layout. An application BIN or OTA package
alone is not an initial installer. There is currently no automated first-install
or recovery tool in this repository. Preserve a full device backup and leave
eFuses unchanged. See the installation guide before using build outputs.

## Relationship to upstream

The port starts from Tasmota development commit
`8a7e815f6de5ad3822e9f28dd06f72d442e90c6c`. Upstream history and attribution are
retained. The original [upstream README](README.upstream.md), ESP-oriented build
files and libraries remain for context; they do not describe MT7697 support.
Report MediaTek-specific problems in this fork rather than upstream Tasmota.

Tasmota's [GPL-3.0 license](LICENSE.txt) is unchanged. Existing third-party notices
remain in their files. Vendor SDK binaries are fetched separately, not committed
or offered as releases. The provenance note records unresolved license coverage
for ten vendor-derived source files; it does not assign those files a new license.
