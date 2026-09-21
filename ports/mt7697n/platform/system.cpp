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

namespace mt7697 {
namespace {
ResetReason boot_reason = ResetReason::Unknown;
}
void capture_reset_reason() {
  switch (hal_wdt_get_reset_status()) {
    case HAL_WDT_TIMEOUT_RESET: boot_reason = ResetReason::Watchdog; break;
    case HAL_WDT_SOFTWARE_RESET: boot_reason = ResetReason::Software; break;
    // Absence of a watchdog cause does not distinguish power-on, external
    // reset or a status already cleared by earlier boot stages.
    default: boot_reason = ResetReason::Unknown; break;
  }
}
ResetReason reset_reason() { return boot_reason; }
const char* reset_reason_text() {
  switch (boot_reason) {
    case ResetReason::Watchdog: return "Watchdog";
    case ResetReason::Software: return "Software reset";
    default: return "Unknown (no retained watchdog cause)";
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
