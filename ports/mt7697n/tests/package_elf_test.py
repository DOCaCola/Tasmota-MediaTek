"""Check package generation against a built application, including mismatches."""
from pathlib import Path
import struct
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from package_ota import validate_elf, make_package, validate_package

BUILD = Path(__file__).resolve().parents[1] / "build/tasmota-gcc13-sdk-runtime"


class ElfTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.image = (BUILD / "tasmota.bin").read_bytes()
        cls.elf = (BUILD / "tasmota.elf").read_bytes()

    def check_rejected_elf(self, data):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "image.elf"
            path.write_bytes(data)
            with self.assertRaises(ValueError):
                validate_elf(self.image, path)

    def test_application(self):
        validate_elf(self.image, BUILD / "tasmota.elf")
        self.assertEqual(validate_package(make_package(self.image)), self.image)

    def test_binary_mismatch(self):
        bad = bytearray(self.image)
        bad[0] ^= 1
        for image in (bad, self.image[:-4], self.image + bytes(4)):
            with self.assertRaises(ValueError):
                validate_elf(image, BUILD / "tasmota.elf")

    def test_wrong_entry(self):
        data = bytearray(self.elf)
        struct.pack_into("<I", data, 24, 0x10071001)
        self.check_rejected_elf(data)

    def test_wrong_machine(self):
        data = bytearray(self.elf)
        struct.pack_into("<H", data, 18, 3)  # x86
        self.check_rejected_elf(data)

    def test_missing_identity_symbol(self):
        data = self.elf.replace(b"mt7697_image_identity\0", b"mt7697_wrong_identity\0")
        self.assertNotEqual(data, self.elf)
        self.check_rejected_elf(data)


if __name__ == "__main__":
    unittest.main()
