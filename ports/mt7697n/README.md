# Experimental native MT7697N / YLXD01YL port foundation

This directory starts a native platform port on Tasmota commit
`8a7e815f6de5ad3822e9f28dd06f72d442e90c6c`, branch `mt7697n-ylxd01yl`.
**The full Tasmota development application now compiles and links.** It has
not run on the lamp. Native HTTP OTA is implemented; GPIO/PWM controls are unavailable in this
bring-up build; the Wi-Fi/settings/MQTT milestone is not yet demonstrated.
The separate probe targets remain SDK integration checks. No ESP8266/ESP32 macros are used to impersonate another MCU.

## Reproduce on Windows

Radio country is a build-time setting in `port_config.h`:

```cpp
#define MT7697_WIFI_COUNTRY "DE"
```

Use a two-letter uppercase country supported by the MediaTek SDK. The default
is Germany for this lamp. The native application/platform build passes it to
the SDK before Wi-Fi initializes, preserving other initialization options.
It does not change eFuses and is independent of language/timezone. Rebuild
after changing it; there is no runtime country command. The older standalone
SDK/network probes do not use this native platform setting.

Python 3.12+ is required for safe archive extraction. From this directory:

```text
python bootstrap.py
python build.py
python build.py --network
python build.py --layout lamp
```

The supplied workspace Python is `RnD/.venv/Scripts/python.exe`.
The toolchain and SDK install outside the Tasmota repository under
`port/vendor/`; downloaded archives are under `port/downloads/`.
`bootstrap.py` checks pinned SHA-256 hashes. The compiler checksum comes from
the MediaTek package index; the BSP checksum records the retrieved official
release archive. Existing SDK headers/library binaries remain unmodified.

Build output includes ELF, raw BIN, linker map, build log and JSON metadata
under `build/`. These scripts have **no upload step**.

The default compiler is the reproducible GCC 4.8.3 probe toolchain.
Experimental `--compiler gcc10` and `--compiler gcc13` builds use a tracked
Arduino core revision under `arduino/`; their network targets currently fail
to link against the prebuilt SDK's old newlib dependencies. They are not
working modern Tasmota toolchains. See
`../../../../RnD/modern-toolchain-investigation.md` for acquisition records,
ABI measurements, and next steps.

The working modern-compiler configuration explicitly retains the SDK runtime:

```text
python bootstrap.py --modern
python build.py --network --compiler gcc13-sdk-runtime
python build.py --dependencies --compiler gcc13-sdk-runtime
```

This uses GCC 13 language support with the original C/C++ headers and libraries,
not modern newlib. The dependency target also compiles/links Tasmota's MQTT
client and JSON parser plus runtime checks. It is not the Tasmota application
and has not been run on hardware. Newer standard-library APIs are unavailable.
Result metadata records each linked runtime archive's hash.

| Command | Purpose | Runtime status |
| --- | --- | --- |
| `build.py` | LinkIt-layout serial + lamp PWM compilation | Not for flashing to the lamp |
| `build.py --network` | LinkIt-layout Wi-Fi scan link/size check | Not for flashing to the lamp |
| `build.py --layout lamp` | Application-only serial/PWM candidate at the stock entry address | Compiles; has not been run on hardware |

The lamp layout has a linker assertion for the stock entry at `0x10071000`
(Thumb branch target `0x10071001`) and limits flash output to `0x130000`
bytes. It provides no bootloader, radio, settings or OTA allocation.
It replaces the development-board startup adapter, avoiding that board's
GPIO36 LED initialization. Other SDK startup code still needs hardware
validation. Do not treat compilation as approval to flash.

The SDK Wi-Fi loader uses a compiled-in radio-image pointer `0x30010000`.
The stock radio starts at flash offset `0x8000`; changing only the linker
origin cannot correct this discrepancy. The build deliberately rejects
`--network --layout lamp` until the radio loader and stock-radio compatibility
have been addressed from source.

## PWM backend

`ylxd01yl_pwm` uses raw MCU GPIO31/PWM32 (warm), GPIO32/PWM33 (cold), and
GPIO30/PWM31 (night). It selects the 40 MHz source and 10 kHz period, writes
zero duties before pin muxing, and checks for the expected 4000-count period.
It does not call Arduino's board-pin-based analogWrite.

Day and night are mutually exclusive. Decreasing duties are written before
increasing duties so a channel handover cannot transiently exceed the chosen
combined duty envelope. Daylight currently rejects warm+cold above 4000
counts, a conservative development limit, not a reconstruction of stock
color/brightness calibration. Invalid requests perform no writes. On an
update failure it attempts zero duty and stops all three channels, then
requires reinitialization.

Color-temperature calibration, dimming curves, transitions, Tasmota light
driver integration, and electrical waveform verification remain outstanding.
The probe initializes zero duties only; it does not turn the lamp on.

The host test compiles the production PWM source against a simulated HAL:

```text
g++ -std=c++11 -Wall -Wextra -Werror -Itests tests/pwm_test.cpp ylxd01yl_pwm.cpp -o build/pwm-test.exe
build/pwm-test.exe
```

It checks per-write power-envelope and day/night-exclusion invariants in
both directions, physical channel assignments, zero-before-mux startup,
invalid inputs, and failure handling. It does not replace electrical tests.

## Next platform work

Owner priority: get actual Tasmota online over Wi-Fi and demonstrate a remote
firmware update. Leave eFuses unchanged; no required modification has been
identified. Keep lamp outputs off for this milestone and defer color/dimming.
Preserving stock offsets is optional: a coherent SDK-based layout is also a
candidate, provided its bootloader, radio, application, settings and OTA
regions are established together. Existing lamp-layout probes do not commit
the final port to the stock map.

The acceptance sequence and OTA findings are in
`../../../../RnD/wifi-ota-milestone.md`.

1. Validate the serial/PWM candidate's startup against the stock bootloader
   and establish an application-only programming/recovery procedure.
2. Rebuild the SDK radio-loader component for the lamp layout and establish
   compatibility with its installed radio image. Audit NVDM initialization
   and storage addresses before enabling networking on the lamp.
3. Establish a contemporary compiler for the Tasmota core while preserving
   the SDK library ABI. The recovered GCC 4.8.3 builds the BSP but lacks
   modern C++/preprocessor features used by current Tasmota.
4. Introduce an explicit MT7697 platform boundary for Wi-Fi, system services,
   time, watchdog, settings, and PWM. Integrate serial commands and MQTT
   and a minimal remote-update route. OTA is part of the first functional
   milestone; richer web features and TLS can follow as required.

Native network and settings services are now implemented in `platform/`.
They provide a polled connection lifecycle and verified, banked NVDM storage
for the 4096-byte settings image. The native core save/load and station
lifecycle hooks now call these services. Full application and OTA integration
remain outstanding.

```text
python tests/run.py --cxx C:/msys64/ucrt64/bin/g++.exe
python build.py --platform --compiler gcc13-sdk-runtime
```

The platform probe links all service entry points without calling the
persistence operations. It uses the lamp board initializer with the SDK flash
layout. No probe is a complete Tasmota build or an approved lamp image.
First Wi-Fi startup still includes synchronous SDK initialization; only the
association/retry state machine is polled. NVDM banking handles item-level
write interruption in host tests; device power-loss behavior is unverified.
Details and evidence: `../../../../RnD/native-platform-services.md`.

`--platform` also compiles the real native `TSettings` layout with offset/size
assertions and syntax-checks the core-hook fixtures using the ARM compiler.
The host test runner executes five suites, including actual core save/load
functions and native station hooks. This probe does not compile the full application.
Current integration: `../../../../RnD/native-core-integration.md`.
# Full application build status

`python build.py --application --compiler gcc13-sdk-runtime` now produces the
full native sketch's `tasmota.elf`, `tasmota.bin`, linker map and result metadata.
This is an SDK-layout development artifact, **not a validated installation**.

Native system/network reporting, asynchronous scan, PHY commands, banked
settings and main-task NTP/RTC services are linked. Generic GPIO/PWM controls
and template edits explicitly report unavailable; pin validation prevents
routing MCU numbers through the LinkIt board's Arduino pin table. Upgrade
uses the native HTTP downloader, package verifier and staging/activation backend.
No ESP updater is linked as a substitute.

The build includes ARM Ext-printf, JSON escaping, and native wall-clock and
delay services. Ext-printf's ARM cursor handling follows AAPCS32 and preserves
argument alignment when ordinary floats/length modifiers precede extensions.
The SDK nano runtime still requires Tasmota's pointer-based extensions for
64-bit integer formatting. Compile flags reject missing returns and emit
`.su` stack-usage files; they cannot measure prebuilt SDK call-chain depth.

Eleven host C++ suites and four Python tests pass. A separate ARM execution
test runs the production formatter/clock objects against the SDK C runtime:

```text
python -m pip install -r tests/requirements-arm.txt
python tests/arm_runtime_test.py
```

Build the application first. The test uses Unicorn for CPU execution, not
MediaTek peripheral emulation. It checks register/stack variadic arguments,
extended and float formatting, allocation, truncation and clock rollover.
Hardware boot, Wi-Fi, persistence, stack/heap measurements, retained reboot
state and OTA remain outstanding. Current evidence and artifacts:
`../../../../RnD/full-application-build.md`.

The application build requires Arduino ctags 5.8-arduino11. Use `--ctags PATH`
when it is not installed in the standard Windows Arduino15 tools directory.
Generated declarations use active preprocessed code and preserve source
locations. Application artifacts are named `tasmota.elf` and `tasmota.bin`;
failed builds remove stale application outputs.

The native board template leaves all lamp outputs unassigned. No image from
this stage is a validated lamp installation image.

## OTA package and staging backend

The native backend validates one uncompressed application-only MMM package,
checks the installed SDK bootloader/radio fingerprints before writes, stages
within the dedicated FOTA region and verifies the complete payload by readback.
Activation is separate and never reboots. The final 4 KiB staging sector is
reserved for bootloader markers. SHA-1 checksums are not signatures.

The OTA gate now requires the official GD25Q32-capable loader from MediaTek
commit `53acd43fbee57b034141068969fa643465dfd743`, acquired and hash-checked by
`python bootloader.py` (also run by `bootstrap.py`). It is kept separately in
`port/vendor/linkit-bootloader/`; the original BSP package remains intact.
The BSP 0.10.21 loader lacks GD25Q32CSIG and falls back to 2 MiB geometry.
Do not install this application expecting OTA to work with that old loader.
The replacement uses the same SDK partition addresses. Build-time checks
verify its GD25Q32 entry, partitions and fingerprint; boot/OTA operation
still requires hardware validation. Startup reports physical JEDEC bytes
separately from the application SDK's selected flash geometry.

Build the application, install `tests/requirements-arm.txt`, then:

```text
python package_ota.py build/tasmota-gcc13-sdk-runtime/tasmota.bin build/tasmota-gcc13-sdk-runtime/tasmota-ota.bin
python tests/package_elf_test.py
```

The builder requires the matching `.elf` beside the input BIN (or `--elf`),
verifies its SDK entry address and application identity symbol, and compares
the complete load image to the BIN. It never uploads anything.

Fourteen C++ host suites, seven Python unit tests and five additional ELF tests
pass. HTTP transfer and the `Upgrade` command are integrated; device validation
remains outstanding. Format provenance, failure behavior and limitations:
`../../../../RnD/native-ota-staging.md`.

Set `OtaUrl http://10.9.8.2:8000/tasmota-ota.bin` to a server you control, then
send `Upgrade 1` over serial or MQTT. Replace the example IP/port with your
server. Serve the generated MMM package, not the raw application BIN.
The downloader supports numeric IPv4 HTTP URLs and a `200` response with one
Content-Length. HTTPS, hostnames, redirects, compression and chunked transfer
are explicitly unsupported. There is no signature/authentication in this path;
use a trusted development network and server.

TCP connect/send/receive are nonblocking, with at most 1024 received bytes per
main-loop poll, 15 seconds without progress and five minutes total allowed.
Flash erase/write, installed-layout fingerprinting and final readback remain
synchronous. After validation and successful activation, the command publishes
its result and schedules the normal settings-save/restart path. Failure does
not schedule a restart. Activation failure requires inspection before reboot:
if trigger cleanup also failed, an update might still be armed. Download
failures can be retried; a completed download is held until reboot.

Integration evidence: `../../../../RnD/native-ota-http.md`.

## Standard Wi-Fi Manager and web UI

The native build includes the standard Tasmota web driver. With no SSID it opens
the usual hostname-based setup hotspot at `http://192.168.4.1`, including DHCP,
captive DNS and browser Wi-Fi setup. `WifiConfig 2` requests the same manager.
After saving working credentials and restarting, the web UI listens on the
station's DHCP address. The default AP is open; `WIFI_AP_PASSPHRASE` controls it.
This revision is build/host-tested; AP, DHCP and browser provisioning still need
hardware validation. See `../../../../RnD/tasmota-ap-webserver.md`.

The console, settings pages and native URL OTA are available. Multipart file
uploads/settings restore and GPIO/template configuration are not implemented.
The transport limits requests to 8 KiB and reuses HTTP/1.1 connections unless
the client requests close. Four idle sockets share the request parser and
expire after 15 seconds. Responses are queued (maximum 32 KiB), then drained
without blocking the main loop. Connection reuse avoids accumulating TCP
TIME_WAIT entries in the SDK's separate 36 KiB lwIP pool during browser polling.
HTTP authentication uses the standard Tasmota web password.

`WebStatus` reports accepted connections, completed responses, temporary send
waits, errors, timeouts and response-queue usage. Completed responses can exceed
accepted connections because a socket is reusable; completion means handed to
TCP, not acknowledged by the browser. `TcpStatus` adds receive/ACK counters,
active and TIME_WAIT connection counts, queued buffers and the SDK pool's used,
peak and allocation-error counters. `Last` is port/sequence/receive-next/ACK/
previous-ACK/resulting-ACK; `Stalled` retains the last retransmitting connection.
The diagnostics are read-only and snapshot PCB lists on the TCP/IP thread.
Run `tests/tcp_diagnostics_test.py` after building to validate their ABI against
the linked vendor debug information.

### Wi-Fi station review (2026-09-21)

The native station backend configures WPA2-PSK/AES (or open networks), owns
station DHCP/netif updates on the TCP/IP task, and does not use the Arduino
station event callbacks. Credential trials are staged in RAM, start on a later
main-task tick, and commit SSID slot 1 only after a fresh DHCP lease. Failed
saved-station attempts return to the setup AP after cleanup. Radio changes can
briefly disconnect AP clients. WPA3/SAE is not implemented.

Host coverage includes SDK-call/thread ownership and failure injection, stale
lease rejection, and the production credential-trial functions. AP access,
DHCP clients and scan were previously demonstrated on the lamp; this revised
station/AP-transition path and remote OTA still need hardware validation.
See workspace `RnD/station-connection-investigation.md` for captures and the
hashed next candidate. Do not conflate a passing host test with a working radio.
