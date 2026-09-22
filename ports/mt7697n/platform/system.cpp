// SPDX-License-Identifier: GPL-3.0-or-later
#include "system.h"
#include <stdlib.h>
extern "C" {
#include <FreeRTOS.h>
#include <task.h>
#include <hal_wdt.h>
#include <hal_rtc.h>
#include <hal_sys.h>
extern char __FLASH_segment_start__, __FLASH_segment_end__, __exidx_end;
}

namespace mt7697 {
namespace {
ResetReason boot_reason = ResetReason::Unknown;
// The SDK's whole-chip reset loses WDT_STA. RTC backup survives that reset
// but loses its contents when the lamp's supply is removed (no RTC battery).
// Reserve the last eight of the SDK's 144 backup bytes; no flash/eFuse writes.
constexpr uint16_t boot_offset = 136;
constexpr uint32_t boot_magic = 0x31534154;  // "TAS1", retention format version.
constexpr uint32_t running = 0x314E5552;     // "RUN1"
constexpr uint32_t restarting = 0x31545352;  // "RST1"
struct BootRecord { uint32_t magic, state; };
bool retention_ok = false;
bool write_boot_record(uint32_t state) {
  const BootRecord record{boot_magic, state};
  BootRecord readback{};
  return hal_rtc_set_data(boot_offset, reinterpret_cast<const char*>(&record),
                          sizeof(record)) == HAL_RTC_STATUS_OK &&
         hal_rtc_get_data(boot_offset, reinterpret_cast<char*>(&readback),
                          sizeof(readback)) == HAL_RTC_STATUS_OK &&
         readback.magic == record.magic && readback.state == record.state;
}
}
void capture_reset_reason() {
  BootRecord record{};
  const bool readable =
      hal_rtc_get_data(boot_offset, reinterpret_cast<char*>(&record),
                       sizeof(record)) == HAL_RTC_STATUS_OK;
  const bool retained = readable && record.magic == boot_magic &&
      (record.state == running || record.state == restarting);
  switch (hal_wdt_get_reset_status()) {
    case HAL_WDT_TIMEOUT_RESET: boot_reason = ResetReason::Watchdog; break;
    case HAL_WDT_SOFTWARE_RESET: boot_reason = ResetReason::Software; break;
    default:
      if (!readable) boot_reason = ResetReason::Unknown;
      else if (!retained) boot_reason = ResetReason::PowerOn;
      else if (record.state == restarting) boot_reason = ResetReason::Software;
      else boot_reason = ResetReason::Unknown;  // Unplanned reset while powered.
      break;
  }
  // Consume the restart intent immediately; a later crash must not inherit it.
  retention_ok = readable && write_boot_record(running);
  if (!retention_ok) boot_reason = ResetReason::Unknown;
}
bool prepare_restart() { return retention_ok && write_boot_record(restarting); }
ResetReason reset_reason() { return boot_reason; }
const char* reset_reason_text() {
  switch (boot_reason) {
    case ResetReason::PowerOn: return "Power on";
    case ResetReason::Watchdog: return "Watchdog";
    case ResetReason::Software: return "Software reset";
    default: return retention_ok ? "Unknown (retained powered session)" :
                                  "Unknown (RTC backup unavailable)";
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
