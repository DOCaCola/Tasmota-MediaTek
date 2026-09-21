"""Build/check one uncompressed application-only MediaTek MMM package.

Checksums detect corruption, not authenticity. This never flashes a device.
"""
import argparse
import hashlib
from pathlib import Path
import struct

MAGIC = 0x004D4D4D
HEADER = 156
APP_BASE = 0x79000
APP_LENGTH = 0x1ED000
CAPACITY = 0x18A000 - 4096  # Activation erases the entire final staging sector.
IDENTITY = b"TASMOTA:YLXD01YL:MT7697N:SDK1".ljust(32, b"\0")


def make_package(image: bytes) -> bytes:
    if len(image) < 32 or len(image) % 4 or IDENTITY not in image:
        raise ValueError("Expected an aligned native MT7697N/YLXD01YL SDK-layout image")
    if len(image) > APP_LENGTH or HEADER + len(image) + 20 > CAPACITY:
        raise ValueError("Image exceeds application or staging capacity")
    header = bytearray(136)
    struct.pack_into("<10I", header, 0, MAGIC, 1, HEADER, APP_BASE, len(image),
                     APP_LENGTH, HEADER + len(image), 20, 0, 0)
    return bytes(header) + hashlib.sha1(header).digest() + image + hashlib.sha1(image).digest()


def validate_package(package: bytes) -> bytes:
    if not HEADER + 32 + 20 <= len(package) <= CAPACITY:
        raise ValueError("Invalid package size")
    image = package[HEADER:-20]
    # Canonical reconstruction validates all descriptors, reserved bytes and both
    # hashes. It rejects multi-region, compressed, shifted and trailing data.
    if make_package(image) != package:
        raise ValueError("Invalid application package header/checksum")
    return image


def validate_elf(image: bytes, path: Path) -> None:
    """Require the linked application and exact objcopy load image."""
    from elftools.elf.elffile import ELFFile

    origin = 0x10000000 + APP_BASE
    with path.open("rb") as stream:
        elf = ELFFile(stream)
        if (elf.elfclass != 32 or not elf.little_endian or elf["e_type"] != "ET_EXEC"
                or elf["e_machine"] != "EM_ARM" or elf["e_entry"] != (origin | 1)):
            raise ValueError("ELF must be an ARM application at the SDK entry address")
        symbols = elf.get_section_by_name(".symtab")
        identities = symbols.get_symbol_by_name("mt7697_image_identity") if symbols else None
        if not identities or len(identities) != 1 or identities[0]["st_size"] != 32:
            raise ValueError("ELF lacks the dedicated application identity")
        segments = sorted(
            (int(s["p_paddr"]), s.data()) for s in elf.iter_segments()
            if s["p_type"] == "PT_LOAD" and s["p_filesz"]
        )
        if not segments or segments[0][0] != origin:
            raise ValueError("ELF load image does not start at the SDK application address")
        reconstructed = bytearray()
        for address, data in segments:
            offset = address - origin
            if offset < len(reconstructed) or offset + len(data) > APP_LENGTH:
                raise ValueError("ELF load segments overlap or exceed the application region")
            reconstructed.extend(bytes(offset - len(reconstructed)))
            reconstructed.extend(data)
        identity_offset = int(identities[0]["st_value"]) - origin
        if identity_offset < 0 or reconstructed[identity_offset:identity_offset + 32] != IDENTITY:
            raise ValueError("ELF application identity is invalid")
        if reconstructed != image:
            raise ValueError("BIN does not match the ELF load image")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("image", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--elf", type=Path, help="Matching ELF (default: input with .elf suffix)")
    args = parser.parse_args()
    if args.image.resolve() == args.output.resolve():
        parser.error("Input image and output package must be different files")
    elf_path = args.elf or args.image.with_suffix(".elf")
    if elf_path.resolve() == args.output.resolve():
        parser.error("Output package must not overwrite the ELF")
    image = args.image.read_bytes()
    validate_elf(image, elf_path)
    result = make_package(image)
    validate_package(result)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(result)
    print(f"{len(result)} bytes; SHA-256 {hashlib.sha256(result).hexdigest()}")
