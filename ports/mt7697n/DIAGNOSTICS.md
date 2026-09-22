# Native diagnostics

The normal application keeps exception reporting, Tasmota logging, heap/stack status,
`WebStatus`, `LampStatus`, `RemoteStatus` and a lightweight `TcpStatus` snapshot.
It has no five-second liveness logging, first-loop checkpoints, supplicant
allocation/configuration wrappers or startup JEDEC dump.
Tasmota `SerialLog`/`WebLog` and actionable application errors retain their normal
behavior. Numeric BLE state tracing is compiled only with `--network-trace`.
Receiver faults and pairing results remain ordinary Tasmota messages.

Vendor SDK module logs and raw SDK stdout are disabled by default, independently
of Tasmota's logger. SDK service/UART initialization is retained, and the direct
exception reporter bypasses stdout filtering. Add `--sdk-logs` to restore vendor
output; its build directory gains `-sdk-logs`. The SDK is prebuilt, so suppressing
output does not remove all vendor format strings or raw printf formatting cost.
Bootloader/ROM output is outside the application's logging controls.

`TcpStatus` reports `"Trace":false` in normal builds. It queries TCP connections,
TIME_WAIT, retransmissions, stalled connection details and SDK pool statistics
only when requested. Packet/ACK fields are omitted rather than filled with zero.
The snapshot runs on the TCP/IP task; it does not install packet callbacks.

To investigate DHCP reception or HTTP ACK handling, build explicitly with:

```text
python ports/mt7697n/build.py --application --compiler gcc13-sdk-runtime --network-trace
```

This creates a separate `build/tasmota-gcc13-sdk-runtime-network-trace` directory.
The normal output directory remains `build/tasmota-gcc13-sdk-runtime`.
`result.json` records `network_trace` in both variants.
It also records `sdk_logs`. The two options can be used independently or together.
Network tracing uses its own UART writer and does not enable vendor stdout.

Trace builds add read-only `tcpip_input` / `tcp_input` wrappers, DHCP progress
and BSSID/channel/filter UART reports, and TCP header/checksum/ACK inspection.
`TcpStatus` then reports `"Trace":true` with the additional ingress, checksum,
ACK and rejected-ACK fields. This instrumentation consumes packet-path CPU time
even without a status request. Enable it for investigations, not normal use.

Both application variants automatically verify linked SDK TCP/statistics ABI
and diagnostic selection. `tests/diagnostic_image_test.py` checks the final ELF
for the expected hooks and allocated strings, retained functional wrappers,
fault reporting and status commands. It also runs the actual ARM startup/stdout
code with intercepted hardware calls to verify that all linked SDK log modules
are registered and disabled without writing persistent settings in normal builds,
and SDK stdout is restored only by its option. Debug symbols, maps and stack-usage files
remain available for offline analysis; they do not add logging to the firmware.

Keep DHCP ownership/IP-ready signaling, OTA verification, PWM checks and SDK
initialization independent of the tracing option.
