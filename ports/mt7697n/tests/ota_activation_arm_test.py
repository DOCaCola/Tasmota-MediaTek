"""Check real ARM OTA activation can let a full SDK log queue drain."""
import argparse
import json
from pathlib import Path
from elftools.elf.elffile import ELFFile
from unicorn import Uc, UcError, UC_ARCH_ARM, UC_MODE_THUMB, UC_MODE_MCLASS, UC_HOOK_CODE
from unicorn.arm_const import *

parser = argparse.ArgumentParser()
parser.add_argument("elf", type=Path)
args = parser.parse_args()
with args.elf.open("rb") as f:
    elf = ELFFile(f)
    symbols = {s.name: s["st_value"] for s in elf.get_section_by_name(".symtab").iter_symbols()
               if s["st_shndx"] != "SHN_UNDEF"}
    sections = [(s["sh_addr"], s.data()) for s in elf.iter_sections()
                if s["sh_flags"] & 2 and s["sh_type"] != "SHT_NOBITS"]

def scenario(queued):
    cpu = Uc(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_MCLASS)
    for address, size in [(0x10000000, 0x400000), (0x20000000, 0x40000),
                          (0x100000, 0x10000), (0xE000E000, 0x1000)]:
        cpu.mem_map(address, size)
    for address, data in sections:
        cpu.mem_write(address, data)
    for name, value in [("xSchedulerRunning", 1), ("syslog_init_done", 7),
                        ("syslog_task_ready", 1), ("uxCriticalNesting", 0)]:
        cpu.mem_write(symbols[name], value.to_bytes(4, "little"))
    # Exercise verbose SDK configuration even if the application's normal startup
    # would disable it. This is the configuration used by the failing old image.
    cpu.mem_write(symbols["log_control_block_fota_module_api"] + 4, b"\0\0")
    queue = queued
    calls = []
    marker = False
    blocked = False
    names = ("hal_flash_init", "hal_flash_erase", "hal_flash_write",
             "xTaskGetSchedulerState", "get_current_time_in_ms", "vsniprintf",
             "xQueueGenericSend")
    hooks = {symbols[n] & ~1: n for n in names}
    def hook(uc, address, size, data):
        nonlocal queue, marker, blocked
        name = hooks.get(address)
        if not name:
            return
        r = [uc.reg_read(n) for n in (UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2)]
        result = 0
        if name == "hal_flash_write":
            assert r[0] == 0x3EFE00 and r[2] == 4
            assert bytes(uc.mem_read(r[1], 4)) == b"MMM\0"
            marker = True
        elif name == "hal_flash_erase":
            assert r[:2] == [0x3EF000, 0]
        elif name == "xTaskGetSchedulerState":
            result = 2  # taskSCHEDULER_RUNNING
        elif name == "xQueueGenericSend":
            mask = uc.reg_read(UC_ARM_REG_BASEPRI)
            calls.append({"queued": queue, "wait_ticks": r[2], "basepri": mask,
                          "marker_written": marker})
            if queue == 8 and mask and r[2] == 0xFFFFFFFF:
                blocked = True
                uc.emu_stop()
                return
            if queue == 8:
                queue = 0  # Logger can drain when interrupts/scheduler are enabled.
            queue += 1
            result = 1
        uc.reg_write(UC_ARM_REG_R0, result)
        uc.reg_write(UC_ARM_REG_PC, uc.reg_read(UC_ARM_REG_LR))
    cpu.hook_add(UC_HOOK_CODE, hook)
    cpu.reg_write(UC_ARM_REG_SP, 0x2003F000)
    cpu.reg_write(UC_ARM_REG_LR, 0x10001001)
    cpu.reg_write(UC_ARM_REG_R0, 0x2003E000)
    try:
        cpu.emu_start(symbols["_ZN6mt769712_GLOBAL__N_111SdkOtaFlash8activateEv"] | 1,
                      0x10001000, count=100000)
    except UcError as error:
        raise RuntimeError(f"ARM fault at PC={cpu.reg_read(UC_ARM_REG_PC):08x}") from error
    returned = cpu.reg_read(UC_ARM_REG_PC) == 0x10001000
    assert returned or blocked, hex(cpu.reg_read(UC_ARM_REG_PC))
    return {"initial_queue": queued, "marker_written": marker, "blocked": blocked,
            "returned": returned, "queue_calls": calls}

results = [scenario(n) for n in (0, 7, 8)]
for result in results:
    assert result["marker_written"] and result["returned"] and not result["blocked"], result
    assert len(result["queue_calls"]) == 2
    assert all(call["basepri"] == 0 for call in result["queue_calls"]), result
print("ARM OTA activation: empty/near-full/full SDK logging queues pass with scheduler enabled")
