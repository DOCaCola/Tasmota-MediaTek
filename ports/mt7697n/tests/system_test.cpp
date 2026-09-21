// SPDX-License-Identifier: GPL-3.0-or-later
#include "../platform/system.h"
#include <cassert>
#include <cstring>
#include <cstdio>
extern "C" {
#include <hal_wdt.h>
char __FLASH_segment_start__, __FLASH_segment_end__, __exidx_end;
static hal_wdt_reset_status_t status = HAL_WDT_NONE_RESET;
hal_wdt_reset_status_t hal_wdt_get_reset_status() { return status; }
size_t xPortGetFreeHeapSize() { return 54321; }
size_t xPortGetMinimumEverFreeHeapSize() { return 12345; }
unsigned uxTaskGetStackHighWaterMark(void* task) { assert(!task); return 123; }
int hal_sys_reboot(uint32_t, uint32_t) { assert(false); return -1; }
}
int main() {
  using mt7697::ResetReason;
  assert(mt7697::reset_reason() == ResetReason::Unknown);
  status = HAL_WDT_TIMEOUT_RESET;
  mt7697::capture_reset_reason();
  assert(mt7697::reset_reason() == ResetReason::Watchdog);
  status = HAL_WDT_NONE_RESET; // Later SDK initialization clears the register.
  assert(mt7697::reset_reason() == ResetReason::Watchdog);
  assert(!strcmp(mt7697::reset_reason_text(), "Watchdog"));
  status = HAL_WDT_SOFTWARE_RESET;
  mt7697::capture_reset_reason();
  assert(mt7697::reset_reason() == ResetReason::Software);
  status = HAL_WDT_NONE_RESET;
  mt7697::capture_reset_reason();
  assert(mt7697::reset_reason() == ResetReason::Unknown);
  assert(mt7697::free_heap() == 54321);
  assert(mt7697::minimum_free_heap() == 12345);
  assert(mt7697::stack_low_water_bytes() == 492);
  puts("System tests passed: reset categories, retained snapshot, heap and stack units.");
}
