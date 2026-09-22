"""Execute linked reset policy and SDK startup with reserved SRAM retention."""
import sys
from elftools.elf.elffile import ELFFile
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_MODE_MCLASS
from unicorn.arm_const import *

cpu = Uc(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_MCLASS)
for start, size in ((0x10000000, 0x400000), (0x20000000, 0x40000),
                    (0x00100000, 0x10000), (0xe000e000, 0x1000),
                    (0x83080000, 0x1000)):
    cpu.mem_map(start, size)
with open(sys.argv[1], "rb") as stream:
    elf = ELFFile(stream)
    symbols = {s.name: s["st_value"] for s in elf.get_section_by_name(".symtab").iter_symbols()
               if s["st_shndx"] != "SHN_UNDEF"}
    retained = elf.get_section_by_name(".boot_retained")
    assert retained["sh_type"] == "SHT_NOBITS"
    assert retained["sh_addr"] == 0x2003fff0 and retained["sh_size"] == 16
    assert symbols["mt7697_boot_record"] == symbols["__StackTop"] == 0x2003fff0
    assert symbols["__StackLimit"] >= symbols["__bss_end__"]
    for segment in elf.iter_segments():
        if segment["p_type"] == "PT_LOAD" and segment["p_filesz"]:
            cpu.mem_write(segment["p_paddr"], segment.data())
    for section in elf.iter_sections():
        if section["sh_flags"] & 2 and section["sh_type"] != "SHT_NOBITS":
            cpu.mem_write(section["sh_addr"], section.data())

record = symbols["mt7697_boot_record"]
def put(address, value):
    cpu.mem_write(address, value.to_bytes(4, "little"))

def call(name):
    cpu.reg_write(UC_ARM_REG_SP, symbols["__StackTop"])
    cpu.reg_write(UC_ARM_REG_LR, 0x103ffff1)
    cpu.emu_start(symbols[name] | 1, 0x103ffff0, count=200000)
    assert cpu.reg_read(UC_ARM_REG_PC) == 0x103ffff0, name
    return cpu.reg_read(UC_ARM_REG_R0)

def boot(expected):
    call("_ZN6mt769720capture_reset_reasonEv")
    assert call("_ZN6mt769712reset_reasonEv") == expected

boot(0)
call("_ZN6mt769715prepare_restartEv")
intent = bytes(cpu.mem_read(record, 16))
assert intent[:4] == b"TAS2" and intent[8:12] == b"RST1"
# Run actual SDK data copies/BSS clearing from reset through SystemInit entry.
# The loader hands off with the original SRAM-end SP. The application's very
# first instruction must replace it before any stack write.
cpu.reg_write(UC_ARM_REG_SP, 0x20040000)
cpu.emu_start(symbols["Reset_Handler"] | 1, symbols["SystemInit"] & ~1, count=1000000)
assert cpu.reg_read(UC_ARM_REG_PC) == symbols["SystemInit"] & ~1
assert cpu.reg_read(UC_ARM_REG_SP) == symbols["__StackTop"]
assert bytes(cpu.mem_read(record, 16)) == intent
boot(4)
boot(7)  # Restart intent is consumed; an unplanned reset is not a requested one.
put(0x8308003c, 1 << 15)
boot(1)  # Real SDK watchdog-status function executes.
put(0x8308003c, 0)
call("_ZN6mt769715prepare_restartEv")
cpu.mem_write(record, bytes(16))  # Power loss despite a pending restart intent.
boot(0)
print("Linked ARM retention checks passed: stack/linker, SDK startup, cold, warm, watchdog, consumed intent")
