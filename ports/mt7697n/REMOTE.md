# Native Yeelight handheld remote

The native MT7697N driver uses MediaTek GAP/GATT, without ESP32 or NimBLE.
It starts BLE once Wi-Fi/CONNSYS is initialized and ten seconds have elapsed.
Scanning is passive, with controller duplicate filtering disabled. Radio callbacks
copy bounded data; authentication, key storage, decryption and light commands
run from the Tasmota main task.

This lamp has no programmed Bluetooth address in the SDK's expected eFuse
field. The controller public identity uses its existing Wi-Fi MAC (converted
to Bluetooth byte order). Scanning and connections use the separate static-random
address created by SDK `bt_task` after its TRNG warm-up. No eFuse access or
invented factory address is required. SMP pairing is disabled; local SMP keys
are not used.

## Commands

| Command | Meaning |
| --- | --- |
| `RemoteStatus` | Receiver, pairing and error counters |
| `RemotePair 60` | Open a 60-second window for a new remote |
| `RemotePair 0` | Close an idle pairing window |
| `RemoteList` | List occupied slots, product IDs and addresses; no keys |
| `RemoteForget 1` | Remove slot 1, saving before changing the live table |
| `RemoteForget 0` | Remove all paired remotes |

Pairing is explicitly enabled rather than opening automatically at every boot.
The scanner must be in state 4 before opening a window. A success or failure
closes the window; use `RemotePair 60` again after a failed attempt.
An unmarked remote is identified from its advertised product ID.

The current implementation accepts stock PID `0x0153` (339) and `0x03B6` (950),
and the unencrypted pairing object `0x0002` requesting event `0x1001`.
GATT service/characteristic/CCCD handles are discovered, not hardcoded.
First binding authenticates locally before reading and storing the 12-byte
beacon key. Up to ten remote records live in NVDM `Tasmota/ble_remotes`, separate
from ordinary Tasmota configuration. Use RemoteForget to remove these records.
No eFuses are programmed.

Normal controls require an authenticated legacy MiBeacon packet from a stored
remote. Both connectable and nonconnectable advertisements are received.
Repeated sequences are suppressed per remote, matching stock's adjacent
duplicate rule; a retransmission can recover from a full action queue.

## Light behavior and scope

On, off, toggle, brightness +/−, CT, moon, maximum/minimum brightness and
long moon are mapped into the existing Tasmota light commands. Brightness uses
the configured `DimmerStep`; CT steps through 10% of the configured mired range
and reverses at its endpoints. CT has no effect while nightlight is selected.
Long moon selects nightlight at 1% and turns it on. Ordinary moon selection
preserves power and restores the selected group's brightness. Actions request
a 500 ms fade without changing the saved fade/speed settings.

The CT and brightness *step sizes* use Tasmota behavior, not a claim of exact
stock remote adjustment-table equivalence. Rotary gestures, delayed-off/effect
codes, factory reset, internal synthetic commands and newer 16-byte-key
MiBeacon formats are not implemented. Unsupported codes are counted and do
not run arbitrary Tasmota commands.

## Diagnostics

States: 0 off, 1 starting, 2 setting address, 3 starting scan, 4 scanning,
5 stopping scan, 6 connecting, 7 service discovery, 8 characteristic discovery,
9 descriptor discovery, 10 authentication, 11 disconnecting, 12 cancelling,
13 fault.

`RemoteStatus` shows report/drop/invalid/decryption/duplicate/action counters,
successful pairings, failures, latest PID/RSSI/code and remaining pairing time.
Reports counts all raw advertisements; only recognized remote service data
enters the receive queue. Invalid counts rejected recognized frames.

Error values `0xE001` through `0xE007` mean malformed event, timeout, storage,
queue/allocation failure, authentication protocol, random/address initialization,
and missing/incompatible GATT handles. Other values are SDK errors.
The last error is retained for diagnosis even after successful scan recovery.
A startup or cleanup fault requires a device restart; it is not silently retried.

Bounded resources: 16 advertising events, 8 control events, 8 light actions,
two SDK connection control blocks, ten SDK timers, 256-byte TX and 1024-byte
RX buffers, and the vendor's 4096-byte BLE task stack. One main-loop pass
processes at most 8 controls, 4 advertisements and 1 light action.
The overall pairing deadline is 30 seconds; GATT stages have a 5-second deadline
and connection establishment has 10 seconds. Timing comparisons tolerate
millisecond-counter wrap.

## Validation

`tests/remote_test.py` builds the portable protocol, production native backend
with real SDK BLE types and substituted OS/radio/storage boundaries, and the
actual Tasmota driver with a command fixture. It is included in `tests/run.py`.
Beacon vectors are synthetic and were verified against stock ARM instructions.
Tests cover early notifications, owned callback data, discovery, authentication,
key persistence and write failure, bad packets/tags, disconnect/cancellation
races, timeouts, duplicates, nonconnectable advertisements and light mappings.

On the YLXD01YL, an unmarked PID `0x0153` handheld paired successfully and its
owner confirmed light control. Hardware counters verified decryption, duplicate
suppression and 35 actions. The saved remote loaded after a software restart,
and decrypted another 132 packets / executed 26 actions without re-pairing,
with BLE scanning and Wi-Fi/HTTP operational again. Cold power-cycle persistence,
every long-press mapping and other remote models remain unverified. An initial
concurrent HTTP test had one connection closed without a response in 90 requests;
this is recorded separately from the successful BLE test.
A repeat three-client HTTP check passed all 180 requests; the earlier closure
remains unreproduced.
See workspace `RnD/native-remote-implementation.md` for the deployment record.
