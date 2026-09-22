// SPDX-License-Identifier: GPL-3.0-or-later
#include "../platform/system.h"
#include <cassert>
#include <cstring>
#include <cstdio>
extern "C" {
#include <hal_wdt.h>
#include <hal_rtc.h>
char __FLASH_segment_start__, __FLASH_segment_end__, __exidx_end;
static hal_wdt_reset_status_t status = HAL_WDT_NONE_RESET;
static char backup[144]{};
static bool rtc_read_error = false, rtc_write_error = false;
hal_rtc_status_t hal_rtc_get_data(uint16_t offset, char* data, uint16_t size) {
  assert(offset == 136 && size == 8);
  if (rtc_read_error) return HAL_RTC_STATUS_ERROR;
  memcpy(data, backup+offset, size);
  return HAL_RTC_STATUS_OK;
}
hal_rtc_status_t hal_rtc_set_data(uint16_t offset, const char* data, uint16_t size) {
  assert(offset == 136 && size == 8);
  if (rtc_write_error) return HAL_RTC_STATUS_ERROR;
  memcpy(backup+offset, data, size);
  return HAL_RTC_STATUS_OK;
}
hal_wdt_reset_status_t hal_wdt_get_reset_status() { return status; }
size_t xPortGetFreeHeapSize() { return 54321; }
size_t xPortGetMinimumEverFreeHeapSize() { return 12345; }
unsigned uxTaskGetStackHighWaterMark(void* task) { assert(!task); return 123; }
int hal_sys_reboot(uint32_t, uint32_t) { assert(false); return -1; }
}
int main() {
  using mt7697::ResetReason;
  assert(mt7697::reset_reason() == ResetReason::Unknown);
  mt7697::capture_reset_reason();
  assert(mt7697::reset_reason() == ResetReason::PowerOn);
  assert(!strcmp(mt7697::reset_reason_text(), "Power on"));
  assert(mt7697::prepare_restart());
  // SDK whole-chip reboot loses WDT cause, retains RTC backup.
  mt7697::capture_reset_reason();
  assert(mt7697::reset_reason() == ResetReason::Software);
  mt7697::capture_reset_reason();
  assert(mt7697::reset_reason() == ResetReason::Unknown); // Intent consumed.
  assert(mt7697::prepare_restart());
  memset(backup, 0, sizeof(backup)); // Supply lost even with pending intent.
  mt7697::capture_reset_reason();
  assert(mt7697::reset_reason() == ResetReason::PowerOn);
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
  rtc_read_error = true;
  mt7697::capture_reset_reason();
  assert(mt7697::reset_reason() == ResetReason::Unknown);
  assert(!mt7697::prepare_restart());
  rtc_read_error = false;
  rtc_write_error = true;
  memset(backup, 0, sizeof(backup));
  mt7697::capture_reset_reason();
  assert(mt7697::reset_reason() == ResetReason::Unknown);
  assert(!mt7697::prepare_restart());
  rtc_write_error = false;
  mt7697::capture_reset_reason();
  assert(mt7697::reset_reason() == ResetReason::PowerOn);
  assert(mt7697::free_heap() == 54321);
  assert(mt7697::minimum_free_heap() == 12345);
  assert(mt7697::stack_low_water_bytes() == 492);
  puts("System tests passed: cold boot, consumed warm intent, power loss, watchdog, RTC errors, heap and stack units.");
}
