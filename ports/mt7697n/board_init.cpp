// SPDX-License-Identifier: GPL-3.0-or-later
// YLXD01YL startup replaces the LinkIt development-board adapter.
// In particular, do not drive its unrelated GPIO36 "user LED".
extern "C" {
#include <top.h>
#include <hal_flash.h>
#include <hal_uart.h>
#include <hal_pwm.h>
}

extern "C" void init_system() {
  top_xtal_init();
  cmnCpuClkConfigureTo192M();
  cmnSerialFlashClkConfTo64M();
  hal_flash_init();
  // setup() initializes and checks the three lamp PWM channels.
}

extern "C" int __io_putchar(int ch) {
  hal_uart_put_char(HAL_UART_0, ch);
  return ch;
}

extern "C" int __io_getchar() { return 0; }
