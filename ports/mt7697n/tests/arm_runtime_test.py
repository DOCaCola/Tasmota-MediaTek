"""Execute production ARM formatter/clock objects with the SDK C runtime.

Requires unicorn==2.1.4 and pyelftools==0.32. Build --application first.
This checks CPU/runtime behavior, not MediaTek peripherals or radio firmware.
"""
from pathlib import Path
import subprocess
from elftools.elf.elffile import ELFFile
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB
from unicorn.arm_const import UC_ARM_REG_PC, UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_R0, UC_ARM_REG_C1_C0_2, UC_ARM_REG_FPEXC

PORT = Path(__file__).resolve().parents[1]
VENDOR = PORT.parents[2] / "vendor"
TOOLS = VENDOR / "modern-toolchain/xpack-arm-none-eabi-gcc-13.3.1-1.1/bin"
OLD = VENDOR / "toolchain/gcc-arm-none-eabi-4.8.3-2014q1/arm-none-eabi"
BUILD = PORT / "build/tasmota-gcc13-sdk-runtime"
OUT = PORT / "build/arm-tests"
OUT.mkdir(parents=True, exist_ok=True)
machine = ["-mcpu=cortex-m4", "-mthumb", "-mfloat-abi=hard", "-mfpu=fpv4-sp-d16"]
cxx = str(TOOLS / "arm-none-eabi-g++.exe")
obj = OUT / "test.o"
subprocess.run([cxx, *machine, "-Os", "-std=c++17", "-fno-exceptions", "-fno-rtti",
                "-ffunction-sections", "-fdata-sections", "-nostdinc",
                "-isystem", str(OLD / "include"),
                "-isystem", str(OLD / "include/c++/4.8.3"),
                "-isystem", str(OLD / "include/c++/4.8.3/arm-none-eabi/armv7e-m/fpu"),
                "-isystem", str(TOOLS.parent / "lib/gcc/arm-none-eabi/13.3.1/include"),
                "-c", str(PORT / "tests/arm_runtime_test.cpp"), "-o", str(obj)], check=True)
linker = OUT / "test.ld"
linker.write_text("""ENTRY(run_tests)
MEMORY { FLASH(rx) : ORIGIN = 0x10000000, LENGTH = 1M
RAM(rwx) : ORIGIN = 0x20000000, LENGTH = 256K }
SECTIONS {
.text : { *(.text*) *(.rodata*) } > FLASH
.data : { *(.data*) } > RAM
.bss (NOLOAD) : { *(.bss*) *(COMMON) } > RAM
}
""")
objects = [next(BUILD.glob("*-" + name + ".o")) for name in
           ("ext_printf.cpp", "dtostrf.c", "wall_clock.cpp")]
runtime = OLD / "lib/armv7e-m/fpu"
gcc = subprocess.check_output([str(TOOLS / "arm-none-eabi-gcc.exe"), *machine,
                               "-print-libgcc-file-name"], text=True).strip()
elf = OUT / "runtime.elf"
subprocess.run([cxx, *machine, "-nostdlib", "-Wl,--gc-sections", "-Wl,-u,_printf_float",
                "-T" + str(linker), str(obj), *map(str, objects), "-Wl,--start-group",
                str(runtime / "libc_s.a"), str(runtime / "libm.a"), str(runtime / "libnosys.a"),
                gcc, "-Wl,--end-group", "-o", str(elf)], check=True)
cpu = Uc(UC_ARCH_ARM, UC_MODE_THUMB)
cpu.mem_map(0x10000000, 0x100000)
cpu.mem_map(0x20000000, 0x40000)
with elf.open("rb") as stream:
    image = ELFFile(stream)
    for segment in image.iter_segments():
        if segment["p_type"] == "PT_LOAD":
            cpu.mem_write(segment["p_vaddr"], segment.data())
    entry = image.header["e_entry"]
cpu.reg_write(UC_ARM_REG_C1_C0_2, 0xf00000)
cpu.reg_write(UC_ARM_REG_FPEXC, 0x40000000)
cpu.reg_write(UC_ARM_REG_SP, 0x2003fff0)
cpu.reg_write(UC_ARM_REG_LR, 0x100ffff1)
cpu.emu_start(entry | 1, 0x100ffff0, count=5000000)
result = cpu.reg_read(UC_ARM_REG_R0)
assert cpu.reg_read(UC_ARM_REG_PC) == 0x100ffff0, "ARM instruction budget exhausted"
assert result == 0, f"ARM runtime test case {result} failed"
print("ARM runtime passed: varargs register/stack rewriting, formatter, SDK libc, wall clock")
