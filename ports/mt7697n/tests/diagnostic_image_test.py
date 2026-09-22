"""Verify diagnostics selection in the linked image, not just build flags."""
import argparse
from pathlib import Path
from elftools.elf.elffile import ELFFile
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_MODE_MCLASS, UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_PC, UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("elf", type=Path)
parser.add_argument("--network-trace", action="store_true")
parser.add_argument("--sdk-logs", action="store_true")
args = parser.parse_args()
with args.elf.open("rb") as stream:
    elf = ELFFile(stream)
    symbols = {s.name for s in elf.get_section_by_name(".symtab").iter_symbols()
               if s["st_shndx"] != "SHN_UNDEF"}
    allocated = b"".join(s.data() for s in elf.iter_sections()
                         if s["sh_flags"] & 2 and s["sh_type"] != "SHT_NOBITS")
    addresses = {s.name: s["st_value"] for s in elf.get_section_by_name(".symtab").iter_symbols()
                 if s["st_shndx"] != "SHN_UNDEF"}
    sections = [(s["sh_addr"], s.data()) for s in elf.iter_sections()
                if s["sh_flags"] & 2 and s["sh_type"] != "SHT_NOBITS"]

for name in ("wpa_supplicant_add_iface", "os_zalloc", "wpa_config_read",
             "wpa_config_alloc_new_conf", "os_strlcpy"):
    assert "__wrap_" + name not in symbols, name
for name in ("malloc", "calloc", "realloc", "free", "wifi_init"):
    assert "__wrap_" + name in symbols, name
for name in ("tcpip_input", "tcp_input"):
    assert ("__wrap_" + name in symbols) == args.network_trace, name
for marker in (b"SDKTRACE ", b"DBG: Main alive", b"DBG: Loop entered",
               b"DBG: RTC polled", b"DBG: NTP polled", b"DBG: OTA polled",
               b"DBG: Scheduler returned", b"DBG: First loop complete",
               b"FLASH init, JEDEC"):
    assert marker not in allocated, marker
for marker in (b"NET: RX frames", b"NET: station DHCP start", b"NET: BSSID=",
               b'"Ingress":', b'"BadChecksum":', b'"Rejected":', b"BLE: Remote state"):
    assert (marker in allocated) == args.network_trace, marker
assert (b'"Trace":true' if args.network_trace else b'"Trace":false') in allocated
for marker in (b"FAULT ICSR CFSR HFSR MMFAR BFAR PSP", b'"PoolErrors":',
               b'"WebStatus":', b'"RemoteStatus":', b'"LampStatus":'):
    assert marker in allocated, marker

# Execute production board startup with only hardware/SDK calls intercepted.
# This checks registration of every linked SDK logger, no persistent changes,
# and the SDK stdout switch independently of the direct fault/UART path.
cpu = Uc(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_MCLASS)
cpu.mem_map(0x10000000, 0x400000)
cpu.mem_map(0x20000000, 0x40000)
cpu.mem_map(0x00100000, 0x10000)
cpu.mem_map(0xE000E000, 0x1000)
for address, data in sections:
    cpu.mem_write(address, data)
def u32(address):
    return int.from_bytes(cpu.mem_read(address, 4), "little")
def cstr(address):
    out = bytearray()
    while (byte := cpu.mem_read(address + len(out), 1)[0]):
        out.append(byte)
    return out.decode()
registered = set()
filtered = {}
uart = []
fault_callbacks = []
stub_names = ("_ZN6mt769720capture_reset_reasonEv", "top_xtal_init",
              "cmnCpuClkConfigureTo192M", "cmnSerialFlashClkConfTo64M",
              "hal_flash_init", "exception_register_callbacks", "log_uart_init",
              "log_init", "syslog_at_set_filter", "hal_uart_put_char")
stubs = {addresses[n] & ~1: n for n in stub_names if n in addresses}
def hook(uc, address, size, data):
    if address not in stubs:
        return
    name = stubs[address]
    r = [uc.reg_read(n) for n in (UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3)]
    if name == "log_init":
        assert r[:2] == [0, 0]
        pos = r[2]
        while (module := u32(pos)):
            registered.add(cstr(u32(module)))
            pos += 4
    elif name == "syslog_at_set_filter":
        filtered[cstr(r[0])] = tuple(r[1:])
    elif name == "hal_uart_put_char":
        uart.append(tuple(r[:2]))
    elif name == "exception_register_callbacks":
        fault_callbacks.append(u32(r[0]))
    uc.reg_write(UC_ARM_REG_R0, 0)
    uc.reg_write(UC_ARM_REG_PC, uc.reg_read(UC_ARM_REG_LR))
cpu.hook_add(UC_HOOK_CODE, hook)
def call(name, value=0, second=0):
    cpu.reg_write(UC_ARM_REG_SP, 0x2003F000)
    cpu.reg_write(UC_ARM_REG_LR, 0x10001001)
    cpu.reg_write(UC_ARM_REG_R0, value)
    cpu.reg_write(UC_ARM_REG_R1, second)
    cpu.emu_start((addresses[name] if isinstance(name, str) else name) | 1,
                  0x10001000, count=10000)
    assert cpu.reg_read(UC_ARM_REG_PC) == 0x10001000, name
call("init_system")
expected = {cstr(u32(address)) for name, address in addresses.items()
            if name.startswith("log_control_block_")}
assert registered == expected and registered, (registered, expected)
assert filtered == ({} if args.sdk_logs else {n: (1, 3, 0) for n in expected}), filtered
uart.clear()
call("__io_putchar", 65)
assert uart == ([(0, 65)] if args.sdk_logs else []), uart
assert cpu.reg_read(UC_ARM_REG_R0) == 65
# Tasmota's Serial writer bypasses vendor stdout in both configurations.
uart.clear()
call("_ZN9UARTClass5writeEh", 0x2003E000, 66)  # Zeroed instance: UART0.
assert uart == [(0, 66)] and cpu.reg_read(UC_ARM_REG_R0) == 1
uart.clear()
assert len(fault_callbacks) == 1
call(fault_callbacks[0])
assert b"FAULT ICSR CFSR HFSR MMFAR BFAR PSP" in bytes(ch for _, ch in uart)
print("Linked diagnostics selection verified:",
      "network trace" if args.network_trace else "normal image",
      "/ SDK logs" if args.sdk_logs else "/ SDK quiet")
