# Experimental native MT7697N / YLXD01YL port foundation

This directory starts a native platform port on Tasmota commit
`8a7e815f6de5ad3822e9f28dd06f72d442e90c6c`, branch `mt7697n-ylxd01yl`.
**It does not yet build or run the Tasmota application.** The current outputs
are SDK integration probes used to establish the platform before adapting
the Tasmota core. No ESP8266/ESP32 macros are used to impersonate another MCU.

## Reproduce on Windows

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
functions and native station hooks. This does not compile the full application.
Current integration: `../../../../RnD/native-core-integration.md`.
# Full application build status

Native system/network reporting, asynchronous scan, PHY selection, settings
reset boundaries and main-task NTP/RTC services are implemented. Eleven C++
host suites and four Python sketch tests pass. Radio-power changes and
ESP-specific erase resets report unsupported. DNS uses the SDK timeout.

`python build.py --application --compiler gcc13-sdk-runtime` compiles the
active Tasmota sketch but **still fails with seven compiler diagnostics** in
PWM and OTA integration. Remaining library linkage and hardware validation
are also outstanding. Successful probe builds remain probes; no application
ELF/BIN is produced by this stage. See
`../../../../RnD/full-application-build.md` for current evidence and limits.

The application build requires Arduino ctags 5.8-arduino11. Use `--ctags PATH`
when it is not installed in the standard Windows Arduino15 tools directory.
Generated declarations use active preprocessed code and preserve source
locations. Application artifacts are named `tasmota.elf` and `tasmota.bin`;
failed builds remove stale application outputs.

The native board template leaves all lamp outputs unassigned. No image from
this stage is a validated lamp installation image.
