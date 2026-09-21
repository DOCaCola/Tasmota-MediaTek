"""Build and run the production backend's host fault-injection tests."""
import argparse
from pathlib import Path
import subprocess
import re
import sys

HERE = Path(__file__).resolve().parent
PORT = HERE.parent

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cxx", default="g++")
    parser.add_argument("--generate-only", action="store_true",
                        help="Emit verbatim core functions for target compilation checks")
    options = parser.parse_args()
    output = PORT / "build/host-tests"
    output.mkdir(parents=True, exist_ok=True)
    core = PORT.parents[1] / "tasmota"
    settings_source = (core / "tasmota_support/settings.ino").read_text()
    functions = []
    for name in ("GetCfgCrc16", "GetSettingsCrc", "GetCfgCrc32", "GetSettingsCrc32",
                 "SettingsSave", "SettingsLoad"):
        match = re.search(r"^(?:uint16_t|uint32_t|void) " + name +
                          r"\([^;\n]*\) \{.*?^\}", settings_source, re.M | re.S)
        if not match:
            raise RuntimeError("Core function not found: " + name)
        functions.append(match.group())
    (output / "core_settings_functions.inc").write_text("\n\n".join(functions))
    if options.generate_only:
        sys.exit(0)
    subprocess.run([sys.executable, str(HERE / "sketch_test.py")], check=True)
    executable = output / "scan.exe"
    subprocess.run([options.cxx, "-std=c++17", "-Wall", "-Wextra", "-Werror",
                    "-I" + str(HERE / "network_fakes"), str(HERE / "scan_test.cpp"),
                    "-o", str(executable)], check=True)
    subprocess.run([str(executable)], check=True)
    executable = output / "system.exe"
    subprocess.run([options.cxx, "-std=c++17", "-Wall", "-Wextra", "-Werror",
                    "-I" + str(HERE / "system_fakes"), str(HERE / "system_test.cpp"),
                    str(PORT / "platform/system.cpp"), "-o", str(executable)], check=True)
    subprocess.run([str(executable)], check=True)
    # Network reporting uses real Arduino value types and the actual .ino;
    # only SDK calls are replaced to exercise error paths without hardware.
    # This runs after the Arduino C objects have been generated below.
    cases = {
        "ntp-service": [HERE / "ntp_service_test.cpp"],
        "ntp": [HERE / "ntp_test.cpp"],
        "pwm": [HERE / "pwm_test.cpp", PORT / "ylxd01yl_pwm.cpp"],
        "platform": [HERE / "platform_test.cpp", PORT / "platform/network.cpp",
                     PORT / "platform/settings.cpp"],
        "banked-settings": [HERE / "banked_settings_test.cpp", PORT / "platform/settings.cpp",
                            PORT / "platform/banked_settings.cpp"],
        "core-settings": [HERE / "core_settings_test.cpp"],
        "core-network": [HERE / "core_network_test.cpp", PORT / "platform/network.cpp"],
    }
    for name, sources in cases.items():
        executable = output / (name + ".exe")
        subprocess.run([options.cxx, "-std=c++17", "-Wall", "-Wextra", "-Werror",
                        "-I" + str(HERE), "-I" + str(PORT), "-I" + str(core), "-I" + str(output),
                        "-I" + str(PORT.parents[2] / "vendor/linkit/mt7697/libraries/LWiFi/src"),
                        *map(str, sources), "-o", str(executable)], check=True)
        subprocess.run([str(executable)], check=True)
    arduino = PORT / "arduino"
    executable = output / "arduino.exe"
    c_objects = []
    for source in ("itoa.c", "dtostrf.c"):
        obj = output / (source + ".o")
        subprocess.run([options.cxx, "-x", "c", "-std=c99", "-Wall", "-Wextra",
                        "-c", str(arduino / source), "-o", str(obj)], check=True)
        c_objects.append(str(obj))
    subprocess.run([
        options.cxx, "-std=c++17", "-Wall", "-Wextra", "-I" + str(arduino),
        str(HERE / "arduino_test.cpp"),
        *[str(arduino / source) for source in
          ("Print.cpp", "IPAddress.cpp", "WString.cpp")],
        *c_objects, "-o", str(executable)], check=True)
    subprocess.run([str(executable)], check=True)
    executable = output / "network-details.exe"
    subprocess.run([
        options.cxx, "-std=c++17", "-Wall", "-Wextra",
        "-I" + str(HERE / "network_fakes"), "-I" + str(arduino),
        str(HERE / "network_details_test.cpp"),
        *[str(arduino / source) for source in ("Print.cpp", "IPAddress.cpp", "WString.cpp")],
        *c_objects, "-o", str(executable)], check=True)
    subprocess.run([str(executable)], check=True)
