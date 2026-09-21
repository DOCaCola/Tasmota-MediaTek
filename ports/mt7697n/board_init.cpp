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
LOG_CONTROL_BLOCK_DECLARE(wifi);
LOG_CONTROL_BLOCK_DECLARE(common);
}

static log_control_block_t* native_log_modules[] = {
  &LOG_CONTROL_BLOCK_SYMBOL(wifi),
  &LOG_CONTROL_BLOCK_SYMBOL(common),
  nullptr
};

extern "C" void init_system() {
#ifdef TASMOTA_PLATFORM_MT7697N
  mt7697::capture_reset_reason();
#endif
  top_xtal_init();
  cmnCpuClkConfigureTo192M();
  cmnSerialFlashClkConfTo64M();
  hal_flash_init();
  // Keep the BSP's diagnostic service: radio initialization and exception
  // handlers otherwise lose the messages needed to diagnose hardware faults.
  log_uart_init(HAL_UART_0);
  log_init(nullptr, nullptr, native_log_modules);
  // GPIO ownership is deferred to the selected application/probe.
}

extern "C" int __io_putchar(int ch) {
  hal_uart_put_char(HAL_UART_0, ch);
  return ch;
}

extern "C" int __io_getchar() { return 0; }
