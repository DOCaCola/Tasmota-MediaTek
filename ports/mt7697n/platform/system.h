// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <stdint.h>

namespace mt7697 {
// Values match the reset categories used by Tasmota's common core.
enum class ResetReason : uint8_t {
  PowerOn = 0, Watchdog = 1, Software = 4, DeepSleep = 5,
  External = 6, Unknown = 7
};
void capture_reset_reason();
void prepare_restart();
ResetReason reset_reason();
const char* reset_reason_text();
uint32_t free_heap();
uint32_t minimum_free_heap();
uint32_t stack_low_water_bytes();
uint32_t image_size();
uint32_t application_capacity();
[[noreturn]] void restart();
}

// Common core comparisons use these category names on all architectures.
constexpr uint32_t REASON_DEFAULT_RST = static_cast<uint32_t>(mt7697::ResetReason::PowerOn);
constexpr uint32_t REASON_EXT_SYS_RST = static_cast<uint32_t>(mt7697::ResetReason::External);
constexpr uint32_t REASON_DEEP_SLEEP_AWAKE = static_cast<uint32_t>(mt7697::ResetReason::DeepSleep);
