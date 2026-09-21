"""Fetch hash-pinned SDK/toolchain archives for the MT7697N build probes."""
import hashlib
import argparse
from pathlib import Path
import tarfile
import zipfile
from urllib.request import urlopen

WORK = Path(__file__).resolve().parents[3]
PACKAGES = [
    (
        "mediatek_linkit_7697-v0.10.21.tar.bz2",
        "https://github.com/MediaTek-Labs/Arduino-Add-On-for-LinkIt-SDK/releases/download/v0.10.21/mediatek_linkit_7697-v0.10.21.tar.bz2",
        "6544ffc4060cb1855dd47c6273fd5cf38be466732399b68252f1ab1586841ce7",
        "linkit",
    ),
    (
        "gcc-arm-none-eabi-4.8.3-2014q1-windows.tar.gz",
        "https://downloads.arduino.cc/gcc-arm-none-eabi-4.8.3-2014q1-windows.tar.gz",
        "fd8c111c861144f932728e00abd3f7d1107e186eb9cd6083a54c7236ea78b7c2",
        "toolchain",
    ),
]


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--modern", action="store_true",
                        help="Also install GCC 13 for the explicit SDK-runtime build")
    options = parser.parse_args()
    if options.modern:
        PACKAGES.append((
            "xpack-arm-none-eabi-gcc-13.3.1-1.1-win32-x64.zip",
            "https://github.com/xpack-dev-tools/arm-none-eabi-gcc-xpack/releases/download/v13.3.1-1.1/xpack-arm-none-eabi-gcc-13.3.1-1.1-win32-x64.zip",
            "49cda1bf01215f3df0613972d21f9fec3eb79dca82506365e7da2ff74b921733",
            "modern-toolchain",
        ))
    cache = WORK / "downloads"
    cache.mkdir(parents=True, exist_ok=True)
    for name, url, expected, directory in PACKAGES:
        archive = cache / name
        if not archive.exists():
            print("Downloading", name, flush=True)
            with urlopen(url, timeout=120) as response:
                archive.write_bytes(response.read())
        actual = hashlib.sha256(archive.read_bytes()).hexdigest()
        if actual != expected:
            raise SystemExit(f"Hash mismatch for {archive}: {actual}")
        if archive.suffix == ".zip":
            with zipfile.ZipFile(archive) as contents:
                contents.extractall(WORK / "vendor" / directory)
        else:
            with tarfile.open(archive) as contents:
                contents.extractall(WORK / "vendor" / directory, filter="data")
        print("Verified and extracted", name, flush=True)
