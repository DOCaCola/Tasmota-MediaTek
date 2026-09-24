# Contributing to Tasmota-MediaTek

This fork focuses on the native MT7697N platform and the YLXD01YL light driver.
Use this repository's default branch for pull requests; the local development
branch is currently named `mt7697n-ylxd01yl`. Keep upstream Tasmota history and
license notices intact. The upstream contribution policy is retained separately
in [CONTRIBUTING.upstream.md](CONTRIBUTING.upstream.md).

Run relevant checks in the [build guide](ports/mt7697n/README.md). State which
tests ran, whether a real lamp was used, compiler/SDK versions and untested
behavior. A host or ARM-emulation pass is not a hardware validation claim. Keep
platform differences behind native interfaces rather than ESP compatibility defines.

Do not submit credentials, Wi-Fi scans, pairing keys, device dumps, packet
captures, downloaded SDK binaries or generated firmware. Preserve third-party
notices and document provenance when importing code. The ten files noted in
[THIRD_PARTY.md](ports/mt7697n/THIRD_PARTY.md) still require license clarification;
do not silently assign a license to them.

Issues should describe hardware, commit, reproduction steps, expected and actual
behavior, and sanitized diagnostics. Report MediaTek-specific problems here;
upstream Tasmota does not maintain this experimental target. CI runs tests only.
This project does not publish releases or firmware binaries.
