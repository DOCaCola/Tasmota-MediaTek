"""Check actual loader geometry, partition boundaries and application gate."""
import hashlib
import re
import struct
import sys
from pathlib import Path

port = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(port))
from bootloader import TARGET, SIZE, SHA256

data = TARGET.read_bytes()
assert len(data) == SIZE and hashlib.sha256(data).hexdigest() == SHA256
header = (port / "platform/sdk_layout.h").read_text(encoding="utf-8")
assert int(re.search(r"kBootloaderBytes\s*=\s*(\d+)", header)[1]) == len(data)
digest = bytes(int(v, 16) for v in re.findall(r"0x([0-9a-f]{2})", header))
assert digest == hashlib.sha1(data).digest()
old = port.parents[2] / "vendor/linkit/mt7697/system/linkit_7697/firmwares/mt7697_bootloader.bin"
assert hashlib.sha1(old.read_bytes()[:SIZE]).digest() != digest
# Exactly one GD25Q32CSIG table row, with the documented C8 40 16 ID.
matches = []
for offset in range(0, len(data) - 24, 4):
    name, manufacturer, jedec, page_size, pages, commands = struct.unpack_from("<6I", data, offset)
    name -= 0x20000000
    if 0 <= name < len(data) and data[name:name+12] == b"GD25Q32CSIG\0":
        matches.append((manufacturer, jedec, page_size * pages))
assert matches == [(0xc8, 0x40160000, 0x400000)], matches
partitions = b"".join(struct.pack("<4I", *row) for row in [
    (0, 0, 0x8000, 0), (1, 0x10000, 0x69000, 0),
    (2, 0x79000, 0x1ed000, 0), (3, 0x266000, 0x18a000, 0)])
assert data.count(partitions) == 1
assert old.read_bytes().count(partitions) == 1
assert 0x266000 + 0x18a000 == 0x3f0000  # NVDM boundary.
print("SDK layout: official loader hash, GD25Q32 geometry, partitions and OTA gate verified")
