# Installation boundaries and application OTA

## Initial conversion and recovery

This repository provides source builds and application OTA packaging, not an
automated initial installer. The tested YLXD01YL has MT7697N and 4 MiB GD25Q32
flash (JEDEC C8 40 16). A stock Yeelight OTA is not a complete flash backup.
Two matching full stock readbacks were obtained during development; restoring
them has not been demonstrated. No stock firmware or device backup is distributed.

The application uses the SDK layout, not stock flash offsets:

| Region | Flash offset/range |
| --- | --- |
| Compatible MediaTek bootloader | starts at 0x000000 |
| SDK radio image | starts at 0x010000 |
| Application allocation | [0x079000, 0x266000) |
| OTA staging, including final activation sector | [0x266000, 0x3f0000) |
| SDK NVDM/settings region | [0x3f0000, 0x400000) |

The entire last 4 KiB staging sector is reserved for activation; package size is
bounded accordingly. `platform/ota.h`, the SDK linker script, and package checks
are authoritative. Do not mix the stock radio offset 0x8000 with this layout.
The downloaded newer loader explicitly supports the tested GD25Q32; the older
SDK loader fell back to 2 MiB geometry and failed the OTA marker write.

First conversion requires a separately verified serial procedure to install a
coherent loader/radio/application layout and initialize NVDM. Keep device-specific
backups private. Leave eFuses unchanged: RF calibration resides there and no
change was required by this port. Recovery uses the momentary START input held
at power-on; normal boot leaves it unpressed. Serial recovery is not exposed by
a software command. Never attach a non-isolated serial setup to live mains.

## Updating an already converted device

Build the application and matching MMM package as described in [README.md](README.md).
Serve that package from a trusted LAN HTTP server with a numeric IPv4 address.
In the Tasmota console, for example (replace the documentation address):

```text
OtaUrl http://192.0.2.10:8000/tasmota-ota.bin
Upgrade 1
```

The native downloader accepts plain HTTP IPv4 URLs with Content-Length. It does
not implement HTTPS, redirects, chunked responses or hostnames. Firmware hashes
and layout fingerprints detect corruption/incompatibility; they are not digital
signatures. Use a trusted network and source. The package is staged, read back,
verified, activated, and followed by the normal save/restart path. Interrupted
power during installation and recovery from every failure mode are not validated.

Automatic application OTA has succeeded on hardware with the corrected loader
and activation path. Cold-start policy and OTA intent retention still have test
gaps; read [POWER_AND_MQTT.md](POWER_AND_MQTT.md). Web multipart firmware upload
and settings restore are not implemented. Do not select upstream ESP binaries.
