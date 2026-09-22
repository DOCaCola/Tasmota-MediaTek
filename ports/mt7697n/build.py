"""Build native MT7697 probes or the Tasmota application; never upload firmware."""
import hashlib
import argparse
import json
import os
from sketch import add_prototypes
from pathlib import Path
import subprocess
import sys

HERE = Path(__file__).resolve().parent
WORK = HERE.parents[2]
SDK = WORK / "vendor/linkit/mt7697"
TOOLS = WORK / "vendor/toolchain/gcc-arm-none-eabi-4.8.3-2014q1/bin"
BUILD = HERE / "build/sdk-probe"


def command(name, args):
    result = subprocess.run(
        [str(TOOLS / (name + ".exe")), *map(str, args)],
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
    )
    with (BUILD / "build.log").open("a", encoding="utf8") as log:
        log.write(result.stdout)
    if result.returncode:
        errors = [line for line in result.stdout.splitlines()
                  if "error:" in line or "undefined reference" in line]
        print("\n".join(errors[:25]) if errors else result.stdout[-6000:])
        print("Full diagnostics:", BUILD / "build.log")
        raise SystemExit(result.returncode)
    return result.stdout


def main():
    global BUILD, TOOLS
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--network", action="store_true", help="Link the Wi-Fi scan probe")
    parser.add_argument("--dependencies", action="store_true",
                        help="Also link Tasmota MQTT and SDK runtime checks (requires gcc13-sdk-runtime)")
    parser.add_argument("--platform", action="store_true",
                        help="Link native network/settings services; no flash writes are invoked")
    parser.add_argument("--application", action="store_true", help="Build the complete native Tasmota application")
    parser.add_argument("--network-trace", action="store_true",
                        help="Enable packet/ACK and DHCP tracing (native platform only)")
    parser.add_argument("--sdk-logs", action="store_true",
                        help="Enable vendor SDK logs and raw SDK UART stdout")
    parser.add_argument("--ctags", type=Path,
                        default=Path(os.environ.get("LOCALAPPDATA", ".")) /
                        "Arduino15/packages/builtin/tools/ctags/5.8-arduino11/ctags.exe",
                        help="Arduino ctags executable for application declarations")
    parser.add_argument("--layout", choices=["sdk", "lamp"], default="sdk")
    parser.add_argument("--compiler", choices=["legacy", "gcc10", "gcc13", "gcc13-sdk-runtime"],
                        default="legacy", help="Experimental compiler/runtime selection.")
    options = parser.parse_args()
    if options.application:
        if not options.ctags.is_file():
            parser.error("--application requires Arduino ctags; supply its path with --ctags")
        options.platform = True
    if options.platform:
        options.dependencies = True
    if options.network_trace and not options.platform:
        parser.error("--network-trace requires --platform or --application")
    if options.dependencies:
        if options.compiler != "gcc13-sdk-runtime":
            parser.error("--dependencies requires --compiler gcc13-sdk-runtime")
        options.network = True
    if options.layout == "lamp" and options.network:
        parser.error("Lamp Wi-Fi requires rebuilding the SDK radio loader for its stock flash map.")
    if options.network:
        BUILD = HERE / "build/network-probe"
    elif options.layout == "lamp":
        BUILD = HERE / "build/lamp-probe"
    if options.dependencies:
        BUILD = HERE / "build/dependency-probe"
    if options.platform:
        BUILD = HERE / "build/platform-probe"
    if options.application:
        BUILD = HERE / "build/tasmota"
    if options.compiler in ("gcc13", "gcc13-sdk-runtime"):
        TOOLS = WORK / "vendor/modern-toolchain/xpack-arm-none-eabi-gcc-13.3.1-1.1/bin"
        BUILD = BUILD.with_name(BUILD.name + "-" + options.compiler)
    elif options.compiler == "gcc10":
        TOOLS = WORK / "vendor/gcc10/xpack-arm-none-eabi-gcc-10.3.1-2.3/bin"
        BUILD = BUILD.with_name(BUILD.name + "-gcc10")
    if options.network_trace:
        BUILD = BUILD.with_name(BUILD.name + "-network-trace")
    if options.sdk_logs:
        BUILD = BUILD.with_name(BUILD.name + "-sdk-logs")
    BUILD.mkdir(parents=True, exist_ok=True)
    artifact = "tasmota" if options.application else "sdk-probe"
    # A failed build must not leave an earlier successful firmware/result visible.
    for name in (artifact + ".elf", artifact + ".bin", "result.json"):
        (BUILD / name).unlink(missing_ok=True)
    (BUILD / "build.log").write_text("")
    properties = {}
    for line in (SDK / "boards.txt").read_text().splitlines():
        if line.startswith("linkit_7697.") and "=" in line:
            key, value = line.split("=", 1)
            properties[key.removeprefix("linkit_7697.")] = value
    core = SDK / "cores/arduino" if options.compiler == "legacy" else HERE / "arduino"
    variant = SDK / "variants/linkit_7697"
    system = SDK / "system/linkit_7697"
    includes = [core, variant] + [
        system / "src" / value for key, value in properties.items()
        if key.startswith("build.") and key.endswith("_inc")
    ]
    machine = [
        "-mlittle-endian", "-mthumb", "-mcpu=cortex-m4",
        "-mfpu=fpv4-sp-d16", "-mfloat-abi=hard",
    ]
    sdk_runtime = options.compiler == "gcc13-sdk-runtime"
    runtime = WORK / "vendor/toolchain/gcc-arm-none-eabi-4.8.3-2014q1/arm-none-eabi"
    runtime_lib = runtime / "lib/armv7e-m/fpu"
    c_headers = []
    cpp_headers = []
    if sdk_runtime:
        # Select the complete SDK runtime explicitly. Never search modern newlib
        # headers/libraries when producing objects for the SDK's runtime ABI.
        compiler_headers = command("arm-none-eabi-gcc", ["-print-file-name=include"]).strip()
        c_headers = ["-nostdinc", "-isystem", compiler_headers,
                     "-isystem", (runtime / "include").as_posix()]
        cpp_root = runtime / "include/c++/4.8.3"
        cpp_headers = ["-nostdinc++", "-fno-sized-deallocation"]
        for directory in (cpp_root, cpp_root / "arm-none-eabi/armv7e-m/fpu",
                          cpp_root / "backward"):
            cpp_headers += ["-isystem", directory.as_posix()]
    definitions = [
        "PRODUCT_VERSION=7697", "MTK_BSPEXT_ENABLE", "USE_HAL_DRIVER",
        "MTK_NVDM_ENABLE",
        "MTK_DEBUG_LEVEL_ERROR", "MTK_LWIP_ENABLE", "MTK_MINISUPP_ENABLE",
        "MTK_WIFI_API_TEST_CLI_ENABLE", "MTK_WIFI_REPEATER_ENABLE",
        "MTK_WIFI_WPS_ENABLE", 'MBEDTLS_CONFIG_FILE="config-mtk-websocket.h"',
        "SUPPORT_MBEDTLS", "MTK_WIFI_TGN_VERIFY_ENABLE", "PCFG_OS=2",
        "_REENT_SMALL", "F_CPU=192000000L", "ARDUINO=10801",
        "ARDUINO_linkit_7697", "ARDUINO_ARCH_LINKIT_RTOS",
    ]
    if options.network_trace:
        definitions.append("MT7697_NETWORK_TRACE")
    if options.sdk_logs:
        definitions += ["MT7697_SDK_LOGS", "MTK_DEBUG_LEVEL_INFO", "MTK_DEBUG_LEVEL_WARNING"]
    flags = machine + [
        "-Os", "-g", "-ffunction-sections", "-fdata-sections",
        "-fno-builtin", "-fno-strict-aliasing", "-fno-common",
        "-fstack-usage", "-Werror=return-type",
    ] + ["-D" + d for d in definitions] + ["-I" + p.as_posix() for p in includes]
    sources = sorted(
        p for directory in (core, variant) for p in directory.iterdir()
        if p.suffix in (".c", ".cpp", ".S")
    ) + [HERE / "smoke.cpp", HERE / "ylxd01yl_pwm.cpp"]
    if options.layout == "lamp" or options.platform:
        sources = [p for p in sources if p.name != "adapter_layer.c"]
        sources.append(HERE / "board_init.cpp")
    if options.network:
        wifi = SDK / "libraries/LWiFi/src"
        flags += ["-DMT7697_NETWORK_PROBE", "-I" + wifi.as_posix()]
        sources += sorted(p for p in wifi.rglob("*") if p.suffix in (".cpp", ".c"))
    if options.dependencies:
        mqtt = HERE.parents[1] / "lib/default/TasmotaPubSub/src"
        json_parser = HERE.parents[1] / "lib/default/jsmn-shadinger-1.0/src"
        flags += ["-DMT7697_DEPENDENCY_PROBE", "-I" + mqtt.as_posix(),
                  "-I" + json_parser.as_posix()]
        sources += [HERE / "dependency_probe.cpp", mqtt / "PubSubClient.cpp",
                    json_parser / "JsonParser.cpp", json_parser / "jsmn.cpp"]
    if options.platform:
        bear = HERE.parents[1] / "lib/lib_ssl/bearssl-esp8266/src"
        flags += ["-I" + bear.as_posix()]
        sources += [bear / p for p in ("hash/sha1.c", "codec/enc32be.c", "codec/dec32be.c")]
        sources += [bear / p for p in ("aead/ccm.c", "symcipher/aes_common.c",
                    "symcipher/aes_small_enc.c", "symcipher/aes_small_ctrcbc.c")]
        sources = [p for p in sources if p.name != "variant_delay.c"]
        sources += sorted((HERE / "platform").glob("*.cpp"))
        if not options.network_trace:
            sources = [p for p in sources if p.name != "sdk_rx_trace.cpp"]
        sources += [HERE / "core_layout.cpp"]
        flags += ["-DTASMOTA_PLATFORM_MT7697N", "-I" + HERE.as_posix(),
                  "-I" + (HERE.parents[1] / "tasmota").as_posix()]
        # Keep the persistence entry points in the link without calling them.
        flags += ["-DMT7697_PLATFORM_PROBE"]
    if options.application:
        root = HERE.parents[1]
        sketch = root / "tasmota"
        sources = [p for p in sources if p.name not in
                   ("smoke.cpp", "dependency_probe.cpp", "link_check.cpp")]
        flags += ["-DFIRMWARE_LITE"]
        for directory in (root / "lib/default").iterdir():
            if directory.is_dir():
                flags += ["-I" + (directory / "src" if (directory / "src").is_dir() else directory).as_posix()]
        sources += [root / "lib/default/Ext-printf/src/ext_printf.cpp",
                    root / "lib/default/jsmn-shadinger-1.0/src/JsonGenerator.cpp",
                    root / "lib/default/Unishox-Tasmota-1.0/src/unishox.cpp"]
        units = [sketch / "tasmota.ino"] + sorted(sketch.glob("tasmota_*/*.ino"))
        application = BUILD / "tasmota.cpp"
        application.write_text('#include <Arduino.h>\n' +
                               "\n".join('#include "' + p.as_posix() + '"' for p in units))
        sources.append(application)
        preprocessed = BUILD / "tasmota.ii"
        command("arm-none-eabi-g++", flags + ["-std=gnu++17", "-fno-exceptions", "-fno-rtti"] + cpp_headers + c_headers
                + ["-E", application.as_posix(), "-o", preprocessed.as_posix()])
        add_prototypes(preprocessed, options.ctags)
        sources[-1] = preprocessed
    objects = []
    for i, source in enumerate(sources):
        obj = BUILD / f"{i:02d}-{source.name}.o"
        cpp = source.suffix in (".cpp", ".ii")
        standard = "-std=c++11" if options.compiler == "legacy" else "-std=gnu++17"
        language = [standard, "-fno-exceptions", "-fno-rtti"] if cpp else ["-std=gnu99"]
        command("arm-none-eabi-g++" if cpp else "arm-none-eabi-gcc",
                flags + language + (cpp_headers if cpp else []) + c_headers
                + ["-c", source.as_posix(), "-o", obj.as_posix()])
        objects.append(obj.as_posix())
    if options.platform:
        # Verify the actual core hooks under the ARM compiler as well as host
        # execution tests. Fixtures supply the rest of the application context.
        subprocess.run([sys.executable, str(HERE / "tests/run.py"), "--generate-only"], check=True)
        for test in ("core_settings_test.cpp", "core_network_test.cpp"):
            command("arm-none-eabi-g++", flags + ["-std=gnu++17", "-fno-exceptions", "-fno-rtti"]
                    + cpp_headers + c_headers
                    + ["-I" + (HERE / "build/host-tests").as_posix(), "-fsyntax-only",
                       (HERE / "tests" / test).as_posix()])
    libraries = [
        (system / "libs" / value).as_posix()
        for key, value in properties.items() if key.startswith("build.lib_")
    ]
    elf = BUILD / (artifact + ".elf")
    linker = variant / "linkscripts/mt7687_flash.ld"
    if options.layout == "lamp":
        linker = HERE / "ylxd01yl.ld"
    runtime_options = ["--specs=nano.specs"]
    runtime_libraries = ["-lm", "-lstdc++", "-lc", "-lnosys"]
    if sdk_runtime:
        runtime_options = ["-nodefaultlibs"]
        runtime_libraries = [(runtime_lib / name).as_posix() for name in
                             ("libm.a", "libstdc++_s.a", "libc_s.a", "libnosys.a")]
        runtime_libraries += [command("arm-none-eabi-gcc",
                                     machine + ["-print-libgcc-file-name"]).strip()]
    link = machine + runtime_options + [
        "-nostartfiles",
        "-Wl,-wrap=malloc,-wrap=calloc,-wrap=realloc,-wrap=free",
        "-Wl,--check-sections,--gc-sections",
        "-T" + linker.as_posix(), "-Wl,-Map," + (BUILD / (artifact + ".map")).as_posix(),
        "-Wl,-u,_printf_float", "-o", elf.as_posix(),
        "-Wl,--start-group", *objects, *libraries,
        *runtime_libraries, "-Wl,--end-group",
    ]
    if options.platform and not options.application:
        link += ["-Wl,--undefined=mt7697_platform_link_check"]
    if options.platform:
        link += ["-Wl,--wrap=wifi_init"]
        if options.network_trace:
            link += ["-Wl,--wrap=tcpip_input", "-Wl,--wrap=tcp_input"]
        link += ["-Wl,--undefined=mt7697_ota_link_check"]
    if options.application:
        link += ["-Wl,--undefined=mt7697_image_identity"]
    command("arm-none-eabi-gcc", link)
    undefined = command("arm-none-eabi-nm", ["-u", elf.as_posix()])
    if undefined.strip():
        raise SystemExit("Unexpected unresolved ELF symbols:\n" + undefined)
    if options.application:
        subprocess.run([sys.executable, str(HERE / "tests/sdk_layout_test.py")],
                       check=True)
        subprocess.run([sys.executable, str(HERE / "tests/tcp_diagnostics_test.py"),
                        str(elf)], check=True)
        subprocess.run([sys.executable, str(HERE / "tests/diagnostic_image_test.py"),
                        str(elf), *(["--network-trace"] if options.network_trace else []),
                        *(["--sdk-logs"] if options.sdk_logs else [])],
                       check=True)
        subprocess.run([sys.executable, str(HERE / "tests/ota_activation_arm_test.py"),
                        str(elf)], check=True)
        subprocess.run([sys.executable, str(HERE / "tests/boot_retention_arm_test.py"),
                        str(elf)], check=True)
    binary = BUILD / (artifact + ".bin")
    command("arm-none-eabi-objcopy", ["-O", "binary", elf.as_posix(), binary.as_posix()])
    if options.platform:
        symbols = {}
        for line in command("arm-none-eabi-nm", ["-n", elf.as_posix()]).splitlines():
            fields = line.split()
            if len(fields) == 3:
                symbols[fields[2]] = int(fields[0], 16)
        # Runtime image reporting uses the linker extent, so verify it against
        # the artifact, including copied data and executable SRAM sections.
        image_extent = symbols["__exidx_end"] - symbols["__FLASH_segment_start__"]
        if image_extent != binary.stat().st_size:
            binary.unlink()
            raise SystemExit("Runtime image extent does not match binary length")
    if options.application:
        subprocess.run([sys.executable, str(HERE / "package_ota.py"),
                        str(binary), str(BUILD / "tasmota-ota.bin"),
                        "--elf", str(elf)], check=True)
    size = command("arm-none-eabi-size", [elf.as_posix()])
    print(size)
    metadata = {
        "target": ("Native Tasmota SDK-layout candidate; not hardware validated"
                   if options.application else
                   "YLXD01YL serial/PWM build candidate; not hardware validated"
                   if options.layout == "lamp" else
                   "LinkIt SDK layout probe; not a lamp flash image"),
        "compiler": command("arm-none-eabi-gcc", ["--version"]).splitlines()[0],
        "runtime": "SDK GCC 4.8.3 nano C/C++ headers and libraries" if sdk_runtime else "compiler bundled",
        "language": "c++11" if options.compiler == "legacy" else "gnu++17",
        "hardware_validated": False,
        "application": options.application,
        "network_trace": options.network_trace,
        "sdk_logs": options.sdk_logs,
        "dependencies_probe": options.dependencies and not options.application,
        "platform_probe": options.platform and not options.application,
        "binary_bytes": binary.stat().st_size,
        "sha256": hashlib.sha256(binary.read_bytes()).hexdigest(),
        "source_files": len(sources),
    }
    if options.application:
        metadata["capabilities"] = {
            "serial_commands": "implemented; hardware unverified",
            "wifi_mqtt_settings": "implemented; hardware unverified",
            "gpio_pwm": "dedicated YLXD01YL CCT/night PWM driver; electrical validation pending",
            "ota": "HTTP IPv4 download, verified staging and Upgrade command implemented; hardware unverified",
            "retained_reboot_state": "RTC cold/warm classifier; common Tasmota settings; hardware validation required",
        }
        metadata["ctags_sha256"] = hashlib.sha256(options.ctags.read_bytes()).hexdigest()
    if sdk_runtime:
        metadata["runtime_archives"] = {
            str(Path(path).relative_to(WORK)): hashlib.sha256(Path(path).read_bytes()).hexdigest()
            for path in runtime_libraries
        }
    (BUILD / "result.json").write_text(json.dumps(metadata, indent=2))
    print(json.dumps(metadata, indent=2))


if __name__ == "__main__":
    main()
