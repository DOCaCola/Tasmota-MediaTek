// SPDX-License-Identifier: GPL-3.0-or-later
// YLXD01YL startup replaces the LinkIt development-board adapter.
// In particular, do not drive its unrelated GPIO36 "user LED".
#ifdef TASMOTA_PLATFORM_MT7697N
#include "platform/system.h"
#endif
extern "C" {
#include <top.h>
#include <hal_flash.h>
#include <spi_flash.h>
#include <hal_uart.h>
#include <hal_pwm.h>
#include <syslog.h>
#include <exception_handler.h>
LOG_CONTROL_BLOCK_DECLARE(wifi);
LOG_CONTROL_BLOCK_DECLARE(common);
extern const struct chip_info* spi_chip_info;
}

static log_control_block_t* native_log_modules[] = {
  &LOG_CONTROL_BLOCK_SYMBOL(wifi),
  &LOG_CONTROL_BLOCK_SYMBOL(common),
  nullptr
};

static bool boot_uart_owned = false;
extern "C" bool native_uart_takeover(hal_uart_port_t port) {
  if (port != HAL_UART_0 || !boot_uart_owned) return true;
  // Serial takes over the early polling console and installs its RX DMA.
  // SDK log_putchar keeps using polling TX on the same physical UART.
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

// Read-only bring-up evidence, collected before Wi-Fi/tasks use the SFC.
// Report the physical JEDEC response separately from the SDK's selected table.
static void flash_report(hal_flash_status_t initialized) {
  const char* label = "\r\nFLASH init, JEDEC read result, JEDEC bytes, SDK id/jedec/capacity:\r\n";
  while (*label) hal_uart_put_char(HAL_UART_0, *label++);
  fault_hex(static_cast<unsigned>(initialized));
  if (initialized == HAL_FLASH_STATUS_OK) {
    unsigned char id[3] = {};
    const int count = flash_read_jedec_id(id, sizeof(id));
    fault_hex(static_cast<unsigned>(count));
    for (unsigned char byte : id) fault_hex(byte);
    if (spi_chip_info) {
      fault_hex(spi_chip_info->id);
      fault_hex(spi_chip_info->jedec_id);
      fault_hex(spi_chip_info->page_size * spi_chip_info->n_pages);
    }
  }
  hal_uart_put_char(HAL_UART_0, '\r');
  hal_uart_put_char(HAL_UART_0, '\n');
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
  const auto flash_initialized = hal_flash_init();
  exception_config_type fault_callbacks = {fault_report, nullptr};
  exception_register_callbacks(&fault_callbacks);
  // Keep the BSP's diagnostic service: radio initialization and exception
  // handlers otherwise lose the messages needed to diagnose hardware faults.
  boot_uart_owned = log_uart_init(HAL_UART_0) == HAL_UART_STATUS_OK;
  log_init(nullptr, nullptr, native_log_modules);
  flash_report(flash_initialized);
  // GPIO ownership is deferred to the selected application/probe.
}

extern "C" int __io_putchar(int ch) {
  hal_uart_put_char(HAL_UART_0, ch);
  return ch;
}

extern "C" int __io_getchar() { return 0; }
