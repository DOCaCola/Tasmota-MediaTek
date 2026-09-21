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
}

extern "C" void init_system() {
#ifdef TASMOTA_PLATFORM_MT7697N
  mt7697::capture_reset_reason();
#endif
  top_xtal_init();
  cmnCpuClkConfigureTo192M();
  cmnSerialFlashClkConfTo64M();
  hal_flash_init();
  // GPIO ownership is deferred to the selected application/probe.
}

extern "C" int __io_putchar(int ch) {
  hal_uart_put_char(HAL_UART_0, ch);
  return ch;
}

extern "C" int __io_getchar() { return 0; }
