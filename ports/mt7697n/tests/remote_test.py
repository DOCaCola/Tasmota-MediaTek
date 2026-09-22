"""Build portable protocol and native backend tests using real SDK BLE types."""
import argparse
from pathlib import Path
import subprocess

PORT = Path(__file__).resolve().parents[1]
ROOT = PORT.parents[1]
parser = argparse.ArgumentParser()
parser.add_argument("--cxx", default="g++")
args = parser.parse_args()
out = PORT / "build/host-tests"
out.mkdir(parents=True, exist_ok=True)
bear = ROOT / "lib/lib_ssl/bearssl-esp8266/src"
sdk = ROOT.parent / "vendor/linkit/mt7697/system/linkit_7697/src/prebuilt/middleware/MTK/bluetooth/inc"
objects = []
for source in ("aead/ccm.c", "symcipher/aes_common.c", "symcipher/aes_small_enc.c",
               "symcipher/aes_small_ctrcbc.c", "codec/enc32be.c", "codec/dec32be.c"):
    obj = out / ("remote-" + Path(source).name + ".o")
    subprocess.run([args.cxx, "-x", "c", "-std=c99", "-O2", "-I"+str(bear),
                    "-c", str(bear/source), "-o", str(obj)], check=True)
    objects.append(str(obj))
for name in ("remote_protocol", "native_remote"):
    exe = out/(name+".exe")
    subprocess.run([args.cxx, "-std=c++17", "-O2", "-Wall", "-Wextra", "-Werror",
        "-I"+str(bear), "-I"+str(PORT/"tests/remote_fakes"), "-I"+str(sdk),
        str(PORT/"tests"/(name+"_test.cpp")), str(PORT/"platform/remote_protocol.cpp"),
        str(PORT/"platform/remote_beacon.cpp"), *objects, "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
exe = out/"remote_driver.exe"
subprocess.run([args.cxx, "-std=c++17", "-O2", "-Wall", "-Wextra", "-Werror",
                "-I"+str(PORT), str(PORT/"tests/remote_driver_test.cpp"),
                "-o", str(exe)], check=True)
subprocess.run([str(exe)], check=True)
