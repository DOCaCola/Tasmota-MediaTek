"""Exercise linked boot policy and real SDK RTC I/O with modeled registers."""
import sys
from elftools.elf.elffile import ELFFile
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_MODE_MCLASS, UC_HOOK_CODE
from unicorn.arm_const import *

cpu = Uc(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_MCLASS)
for start, size in ((0x10000000,0x400000),(0x20000000,0x40000),
                    (0x00100000,0x10000),(0xe000e000,0x1000),
                    (0x83080000,0x1000),(0x830c0000,0x1000)):
    cpu.mem_map(start,size)
with open(sys.argv[1],"rb") as stream:
    elf=ELFFile(stream)
    symbols={s.name:s["st_value"] for s in elf.get_section_by_name(".symtab").iter_symbols()
             if s["st_shndx"]!="SHN_UNDEF"}
    for s in elf.iter_sections():
        if s["sh_flags"] & 2 and s["sh_type"]!="SHT_NOBITS":
            cpu.mem_write(s["sh_addr"],s.data())
# Only formatting is intercepted; actual SDK unlock/backup/WD status code runs.
def skip_printf(uc,a,size,data):
    uc.reg_write(UC_ARM_REG_R0,0)
    uc.reg_write(UC_ARM_REG_PC,uc.reg_read(UC_ARM_REG_LR))
cpu.hook_add(UC_HOOK_CODE,skip_printf,begin=symbols["printf"]&~1,end=symbols["printf"]&~1)
def put(a,v): cpu.mem_write(a,v.to_bytes(4,"little"))
put(0x830c0100,2)  # RTC G_ENABLE: register bank accessible.
def call(name):
    cpu.reg_write(UC_ARM_REG_SP,0x2003f000)
    cpu.reg_write(UC_ARM_REG_LR,0x103ffff1)
    cpu.emu_start(symbols[name]|1,0x103ffff0,count=200000)
    assert cpu.reg_read(UC_ARM_REG_PC)==0x103ffff0,name
    return cpu.reg_read(UC_ARM_REG_R0)
def boot(expected):
    call("_ZN6mt769720capture_reset_reasonEv")
    assert call("_ZN6mt769712reset_reasonEv")==expected
cpu.mem_write(0x830c0140,bytes([0x42])*136+bytes(8))
boot(0)
assert bytes(cpu.mem_read(0x830c0140,136))==bytes([0x42])*136
assert bytes(cpu.mem_read(0x830c01c8,8))==b"TAS1RUN1"
assert call("_ZN6mt769715prepare_restartEv")==1
assert bytes(cpu.mem_read(0x830c01c8,8))==b"TAS1RST1"
boot(4)  # Requested warm reset with no WDT_STA.
boot(7)  # A later unplanned reset cannot reuse the consumed restart intent.
put(0x8308003c,1<<15)
boot(1)
put(0x8308003c,0)
assert call("_ZN6mt769715prepare_restartEv")==1
cpu.mem_write(0x830c0140,bytes(144))  # Supply lost, despite pending intent.
boot(0)
print("Linked ARM reset/RTC checks passed: cold, warm, watchdog, consumed intent, backup isolation")
