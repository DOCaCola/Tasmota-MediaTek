"""Acquire the GD25Q32-capable official loader; never program a device."""
import hashlib
from pathlib import Path
from urllib.request import urlopen

COMMIT = "53acd43fbee57b034141068969fa643465dfd743"
URL = ("https://raw.githubusercontent.com/MediaTek-Labs/Arduino-Add-On-for-LinkIt-SDK/"
       + COMMIT + "/middleware/third_party/arduino/hardware/tools/mt7687/flash_tool/mt7697_bootloader.bin")
SHA256 = "6b7bfa90dbb4ec89e22b6fbe7974c7a7d26f965bde5bfba83b6f76ac3adfbd5c"
SIZE = 27232
TARGET = Path(__file__).resolve().parents[3] / "vendor/linkit-bootloader/mt7697_bootloader.bin"


def acquire():
    data = TARGET.read_bytes() if TARGET.exists() else urlopen(URL, timeout=30).read()
    if len(data) != SIZE or hashlib.sha256(data).hexdigest() != SHA256:
        raise ValueError("Official bootloader size/hash mismatch")
    TARGET.parent.mkdir(parents=True, exist_ok=True)
    TARGET.write_bytes(data)
    return TARGET


if __name__ == "__main__":
    print(acquire())
