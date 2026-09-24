# Dependency provenance and license review

## Source tree

Tasmota is retained with its [GPL-3.0 license](../../LICENSE.txt), history,
authorship and existing third-party notices. This fork began at commit
`8a7e815f6de5ad3822e9f28dd06f72d442e90c6c`. New native files carrying
`SPDX-License-Identifier: GPL-3.0-or-later` retain that designation.

The adapted `arduino/` core came from `cores/arduino` in the official MediaTek
LinkIt 7697 BSP v0.10.21 archive. Its original Arduino LGPL-2.1-or-later headers
and individual permissive/public-domain notices are retained. A copy of
[LGPL-2.1](arduino/COPYING.LESSER) accompanies those notices. Git history records
the local changes (modern compiler support and native platform services).
Adding this license text does not relicense files with other or missing notices.

### Unresolved source-license coverage

Ten imported files have no license statement in the pinned BSP source:

- `arduino/delay.c`, `arduino/delay.h`
- `arduino/pin_mux.c`, `arduino/pin_mux.h`
- `arduino/Tone.cpp`, `arduino/Tone.h`
- `arduino/wiring_digital.c`, `arduino/wiring_digital.h`
- `arduino/wiring_interrupt.c`, `arduino/wiring_interrupt.h`

`pin_mux.c` and `Tone.cpp` include local edits; the other eight match the BSP
source after newline normalization. The official
[add-on repository](https://github.com/MediaTek-Labs/Arduino-Add-On-for-LinkIt-SDK)
contains no root license at inspected commit
`53acd43fbee57b034141068969fa643465dfd743`. We have not established a covering
grant for these ten files. Their presence in a public vendor repository is not
being treated as a blanket license. Resolve provenance/license coverage before
publishing this fork's existing history. Removing a file from the current tree
alone does not remove it from earlier commits. No licensing claim is made here
about redistributing linked vendor binaries.

## Separately downloaded dependencies

`bootstrap.py` pins URLs and archive SHA-256 values. Archives and extracted SDKs
live outside the checkout; no generated firmware, vendor libraries, bootloader,
radio image, or device flash backup is added to Git by the port.

| Component | Version/source | Use |
| --- | --- | --- |
| MediaTek BSP | [v0.10.21](https://github.com/MediaTek-Labs/Arduino-Add-On-for-LinkIt-SDK/releases/tag/v0.10.21) | SDK headers, libraries, radio and runtime |
| GNU Arm toolchain | Arduino GCC 4.8.3 2014q1 package | Original SDK C/C++ headers/libraries; legacy probes |
| xPack GNU Arm | 13.3.1-1.1 Windows x64 | Modern compiler with original SDK runtime |
| Arduino ctags | 5.8-arduino11 Windows | Sketch declarations; archive pin from Arduino package index |
| Newer MediaTek bootloader | commit 53acd43fbee57b034141068969fa643465dfd743 | GD25Q32-capable loader, independently hash-pinned in bootloader.py |
| Unicorn / pyelftools | tests/requirements-arm.txt | Local ARM and ELF checks; not linked into firmware |

Downloaded components retain their own licenses and notices. The vendor
[build description](https://github.com/MediaTek-Labs/Arduino-Add-On-for-LinkIt-SDK#build-prerequisites)
explains that its add-on requires the separate LinkIt SDK and combines source
with prebuilt platform libraries. This project does not claim that all of those
components are GPL or distribute a firmware release containing them.

Stock-derived calibration/transition test vectors are included; stock firmware
images, personal configurations and paired-device secrets are not.
