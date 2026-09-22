// SPDX-License-Identifier: GPL-3.0-or-later
#include "system.h"
#include <stdlib.h>
extern "C" {
#include <FreeRTOS.h>
#include <task.h>
#include <hal_wdt.h>
#include <hal_sys.h>
extern char __FLASH_segment_start__, __FLASH_segment_end__, __exidx_end;
}

// Fixed, NOLOAD region excluded from the application stack and startup clearing.
// The SDK bootloader uses low SRAM and switches SP without pushing before entry.
extern "C" {
__attribute__((section(".boot_retained"), used)) volatile uint32_t mt7697_boot_record[4];
}
namespace mt7697 {
namespace {
ResetReason boot_reason = ResetReason::Unknown;
constexpr uint32_t boot_magic = 0x32534154;  // "TAS2", retention format version.
constexpr uint32_t running = 0x314E5552;
constexpr uint32_t restarting = 0x31545352;
void write_boot_record(uint32_t state) {
  mt7697_boot_record[0] = 0;  // Commit the signature last.
  mt7697_boot_record[1] = ~boot_magic;
  mt7697_boot_record[2] = state;
  mt7697_boot_record[3] = ~state;
  __sync_synchronize();
  mt7697_boot_record[0] = boot_magic;
  __sync_synchronize();
}
}
void capture_reset_reason() {
  const uint32_t state = mt7697_boot_record[2];
  const bool retained = mt7697_boot_record[0] == boot_magic &&
      mt7697_boot_record[1] == ~boot_magic && mt7697_boot_record[3] == ~state &&
      (state == running || state == restarting);
  switch (hal_wdt_get_reset_status()) {
    case HAL_WDT_TIMEOUT_RESET: boot_reason = ResetReason::Watchdog; break;
    case HAL_WDT_SOFTWARE_RESET: boot_reason = ResetReason::Software; break;
    default:
      if (!retained) boot_reason = ResetReason::PowerOn;
      else if (state == restarting) boot_reason = ResetReason::Software;
      else boot_reason = ResetReason::Unknown;  // Unplanned reset while powered.
      break;
  }
  // Consume intent immediately; subsequent unplanned resets remain distinct.
  write_boot_record(running);
}
void prepare_restart() { write_boot_record(restarting); }
ResetReason reset_reason() { return boot_reason; }
const char* reset_reason_text() {
  switch (boot_reason) {
    case ResetReason::PowerOn: return "Power on";
    case ResetReason::Watchdog: return "Watchdog";
    case ResetReason::Software: return "Software reset";
    default: return "Unknown (retained powered session)";
  }
}
uint32_t free_heap() { return xPortGetFreeHeapSize(); }
uint32_t minimum_free_heap() { return xPortGetMinimumEverFreeHeapSize(); }
uint32_t stack_low_water_bytes() {
  return uxTaskGetStackHighWaterMark(nullptr) * sizeof(StackType_t);
}
uint32_t image_size() {
  return reinterpret_cast<uintptr_t>(&__exidx_end) -
         reinterpret_cast<uintptr_t>(&__FLASH_segment_start__);
}
uint32_t application_capacity() {
  return reinterpret_cast<uintptr_t>(&__FLASH_segment_end__) -
         reinterpret_cast<uintptr_t>(&__FLASH_segment_start__);
}
[[noreturn]] void restart() {
  hal_sys_reboot(HAL_SYS_REBOOT_MAGIC, 0);
  abort(); // A full-system reboot must not return.
}
}
