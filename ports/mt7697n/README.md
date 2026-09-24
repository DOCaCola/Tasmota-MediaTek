# Native MT7697N / YLXD01YL port

The native application has been exercised on a YLXD01YL with Wi-Fi, MQTT, browser
setup, application OTA, CCT/night PWM and a paired BLE remote. This guide describes
the current build; it supersedes the early serial/network probe instructions.
See the [project overview](../../README.md) for tested features and limitations.

## Windows x64 application build

Prerequisites: Python 3.12 or newer, Git, and a host C++17 compiler for tests
(for example MSYS2 UCRT64 GCC). Commands below run from the repository root in
Bash. The full application compiler and Arduino ctags are downloaded by bootstrap;
an Arduino IDE installation is not required.

```sh
python -m venv .venv
.venv/Scripts/python.exe -m pip install -r ports/mt7697n/tests/requirements-arm.txt
.venv/Scripts/python.exe ports/mt7697n/bootstrap.py --modern
.venv/Scripts/python.exe ports/mt7697n/tests/run.py --cxx C:/msys64/ucrt64/bin/g++.exe
.venv/Scripts/python.exe ports/mt7697n/build.py --application --compiler gcc13-sdk-runtime
```

`bootstrap.py` verifies SHA-256 pins before extraction. It writes `vendor/` and
`downloads/` **next to this checkout**, not inside it. Keep the checkout in a
writable parent directory. The relative layout is independent of the repository
name. The SDK runtime is deliberately retained: a modern newlib cannot replace
its ABI. See [dependency provenance](THIRD_PARTY.md) for versions and sources.

The default ctags path is the bootstrapped sibling
`vendor/ctags/ctags-5.8-arduino11/ctags.exe`; `--ctags PATH` can override it.
Build scripts read source files as UTF-8 regardless of the Windows locale.

Before a hardware build, set `MT7697_WIFI_COUNTRY` in `port_config.h` to the
appropriate SDK-supported two-letter country. It defaults to `DE`; this is a
build-time radio setting, independent of timezone, and does not modify eFuses.
Do not commit Wi-Fi credentials, broker passwords or device settings.

Application outputs are under `ports/mt7697n/build/tasmota-gcc13-sdk-runtime/`:
`tasmota.elf`, `tasmota.bin`, map, stack-usage files, build log and `result.json`.
The metadata records dependency hashes. Build timestamps mean separate builds
are not promised to be byte-for-byte identical. Build scripts never flash.

## Verification and host CI

The host suite runs actual port code with simulated hardware and tests common
Tasmota settings/power functions. On Linux, install Python 3.12+ and g++, then:

```sh
python ports/mt7697n/bootstrap.py --sdk-only
python ports/mt7697n/tests/run.py --cxx g++
```

SDK-only setup supplies real SDK headers used by the tests without downloading
Windows compilers. The GitHub workflow runs these host checks with read-only
repository permissions and does not create releases or upload firmware.

The full Windows application build also executes linked SDK layout/ABI,
diagnostics, OTA activation and retained-SRAM startup checks. After building:

```sh
.venv/Scripts/python.exe ports/mt7697n/tests/arm_runtime_test.py
.venv/Scripts/python.exe ports/mt7697n/tests/package_elf_test.py
```

Unicorn checks ARM instructions with substituted peripheral boundaries. These
tests do not emulate a full radio or establish hardware reliability.

## OTA packaging

```sh
.venv/Scripts/python.exe ports/mt7697n/package_ota.py ports/mt7697n/build/tasmota-gcc13-sdk-runtime/tasmota.bin ports/mt7697n/build/tasmota-gcc13-sdk-runtime/tasmota-ota.bin
```

The packager requires the matching ELF and checks its load image. This is an
application-only MMM package for an already converted device with the expected
SDK layout and verified loader/radio fingerprints. It is not an initial flash
image. Follow [INSTALLATION.md](INSTALLATION.md) before attempting an update.

## Runtime documentation

[Light driver](LAMP.md), [power and MQTT](POWER_AND_MQTT.md), [remote](REMOTE.md),
and [diagnostics](DIAGNOSTICS.md) describe the native interfaces. Generic GPIO
and ESP templates are not supported: the lamp uses fixed GPIO31 warm, GPIO32
cold and GPIO30 night PWM, with day/night exclusion.

Older `build.py` serial/network/dependency/platform targets remain development
probes. The default compiler selection is legacy GCC for those probes; the
application command must explicitly select `gcc13-sdk-runtime`. The historical
`--layout lamp` probe is not the deployed layout and must not be flashed as an
application installer. Full Linux/macOS cross-build support is not implemented.
