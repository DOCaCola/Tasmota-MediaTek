// SPDX-License-Identifier: GPL-3.0-or-later
// YLXD01YL startup replaces the LinkIt development-board adapter.
// In particular, do not drive its unrelated GPIO36 "user LED".
#ifdef TASMOTA_PLATFORM_MT7697N
#include "platform/system.h"
#endif
extern "C" {
#include <top.h>
#include <hal_flash.h>
#include <hal_uart.h>
#include <hal_pwm.h>
#include <syslog.h>
#include <exception_handler.h>
LOG_CONTROL_BLOCK_DECLARE(wifi);
LOG_CONTROL_BLOCK_DECLARE(common);
LOG_CONTROL_BLOCK_DECLARE(connsys);
LOG_CONTROL_BLOCK_DECLARE(lwip);
LOG_CONTROL_BLOCK_DECLARE(inband);
LOG_CONTROL_BLOCK_DECLARE(minisupp);
LOG_CONTROL_BLOCK_DECLARE(dhcpd);
LOG_CONTROL_BLOCK_DECLARE(fota_module_api);
LOG_CONTROL_BLOCK_DECLARE(BT);
LOG_CONTROL_BLOCK_DECLARE(BTMM);
LOG_CONTROL_BLOCK_DECLARE(BTHCI);
LOG_CONTROL_BLOCK_DECLARE(BTL2CAP);
LOG_CONTROL_BLOCK_DECLARE(BTRFCOMM);
LOG_CONTROL_BLOCK_DECLARE(BTSPP);
LOG_CONTROL_BLOCK_DECLARE(BTIF);
}

static log_control_block_t* native_log_modules[] = {
  &LOG_CONTROL_BLOCK_SYMBOL(wifi),
  &LOG_CONTROL_BLOCK_SYMBOL(common),
  &LOG_CONTROL_BLOCK_SYMBOL(connsys),
  &LOG_CONTROL_BLOCK_SYMBOL(lwip),
  &LOG_CONTROL_BLOCK_SYMBOL(inband),
  &LOG_CONTROL_BLOCK_SYMBOL(minisupp),
  &LOG_CONTROL_BLOCK_SYMBOL(dhcpd),
  &LOG_CONTROL_BLOCK_SYMBOL(fota_module_api),
  &LOG_CONTROL_BLOCK_SYMBOL(BT),
  &LOG_CONTROL_BLOCK_SYMBOL(BTMM),
  &LOG_CONTROL_BLOCK_SYMBOL(BTHCI),
  &LOG_CONTROL_BLOCK_SYMBOL(BTL2CAP),
  &LOG_CONTROL_BLOCK_SYMBOL(BTRFCOMM),
  &LOG_CONTROL_BLOCK_SYMBOL(BTSPP),
  &LOG_CONTROL_BLOCK_SYMBOL(BTIF),
  nullptr
};

static bool boot_uart_owned = false;
extern "C" bool native_uart_takeover(hal_uart_port_t port) {
  if (port != HAL_UART_0 || !boot_uart_owned) return true;
  // Serial takes over the early polling console and installs its RX DMA.
  // Optional SDK stdout uses polling TX on the same physical UART.
  if (hal_uart_deinit(port) != HAL_UART_STATUS_OK) return false;
  boot_uart_owned = false;
  return true;
}

// Exception callbacks run after the SDK makes XIP flash readable again.
// Use polling UART directly: the scheduler/logger may be the failing component.
static void fault_hex(unsigned value) {
  for (int shift = 28; shift >= 0; shift -= 4) {
    const unsigned digit = (value >> shift) & 15;
    hal_uart_put_char(HAL_UART_0, digit < 10 ? '0' + digit : 'a' + digit - 10);
  }
  hal_uart_put_char(HAL_UART_0, ' ');
}

static void fault_report() {
  const char* label = "\r\nFAULT ICSR CFSR HFSR MMFAR BFAR PSP:\r\n";
  while (*label) hal_uart_put_char(HAL_UART_0, *label++);
  const unsigned registers[] = {0xe000ed04u, 0xe000ed28u, 0xe000ed2cu,
                                0xe000ed34u, 0xe000ed38u};
  for (unsigned address : registers) {
    fault_hex(*reinterpret_cast<volatile unsigned*>(address));
  }
  unsigned psp;
  __asm volatile("mrs %0, psp" : "=r"(psp));
  fault_hex(psp);
  // Raw exception frame, including any FP frame. Bound reads to system SRAM
  // because stack corruption itself can be the cause of the exception.
  if (!(psp & 3) && psp >= 0x20000000 && psp <= 0x20040000 - 32 * 4) {
    label = "\r\nPSP words:\r\n";
    while (*label) hal_uart_put_char(HAL_UART_0, *label++);
    for (unsigned i = 0; i < 32; ++i)
      fault_hex(reinterpret_cast<volatile unsigned*>(psp)[i]);
  }
  hal_uart_put_char(HAL_UART_0, '\r');
  hal_uart_put_char(HAL_UART_0, '\n');
}

extern "C" void init_system() {
#ifdef TASMOTA_PLATFORM_MT7697N
  mt7697::capture_reset_reason();
#endif
  top_xtal_init();
  cmnCpuClkConfigureTo192M();
  cmnSerialFlashClkConfTo64M();
  hal_flash_init();
  exception_config_type fault_callbacks = {fault_report, nullptr};
  exception_register_callbacks(&fault_callbacks);
  // Retain SDK service/UART initialization. Filter vendor output separately
  // from Tasmota's SerialLog/WebLog and the direct exception reporter.
  boot_uart_owned = log_uart_init(HAL_UART_0) == HAL_UART_STATUS_OK;
  log_init(nullptr, nullptr, native_log_modules);
#ifndef MT7697_SDK_LOGS
  // Use the SDK API: its private short-enum layout differs from our headers.
  // Only the first (module-name pointer) field is accessed here.
  for (auto** module=native_log_modules; *module; ++module) {
    syslog_at_set_filter(const_cast<char*>((*module)->module_name),
                        DEBUG_LOG_OFF, PRINT_LEVEL_ERROR, 0);
  }
#endif
  // GPIO ownership is deferred to the selected application/probe.
}

extern "C" int __io_putchar(int ch) {
#ifdef MT7697_SDK_LOGS
  hal_uart_put_char(HAL_UART_0, ch);
#endif
  return ch;
}

extern "C" int __io_getchar() { return 0; }
