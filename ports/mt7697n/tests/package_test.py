import sys
from pathlib import Path
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from package_ota import make_package, validate_package, IDENTITY, CAPACITY, HEADER

class PackageTests(unittest.TestCase):
    def test_roundtrip(self):
        # Identity crosses a verifier's 1024-byte read boundary.
        image = bytearray(8192)
        image[1010:1010 + len(IDENTITY)] = IDENTITY
        packet = make_package(bytes(image))
        self.assertEqual(validate_package(packet), image)
        output = Path(__file__).resolve().parents[1] / "build/host-tests/ota-fixture.bin"
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_bytes(packet)

    def test_bounds(self):
        with self.assertRaises(ValueError): make_package(bytes(100))
        with self.assertRaises(ValueError): make_package(IDENTITY + b"x")
        with self.assertRaises(ValueError): make_package(IDENTITY + bytes(CAPACITY))
        image = IDENTITY + bytes(CAPACITY - HEADER - 20 - len(IDENTITY))
        self.assertEqual(len(make_package(image)), CAPACITY)

    def test_mutations(self):
        packet = make_package(IDENTITY + bytes(100))
        for i in range(len(packet)):
            bad = bytearray(packet); bad[i] ^= 1
            with self.assertRaises(ValueError): validate_package(bytes(bad))
        for bad in (packet[:-1], packet + b"\0", b""):
            with self.assertRaises(ValueError): validate_package(bad)

if __name__ == "__main__":
    unittest.main()
