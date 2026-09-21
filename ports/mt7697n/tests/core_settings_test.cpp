// SPDX-License-Identifier: GPL-3.0-or-later
// Compiles the actual core's save/load/CRC functions, generated verbatim by run.py.
#include <cstdint>
#include <cstring>
#include <cassert>
#include <cstdio>
#include <initializer_list>
#define PROGMEM
#define PSTR(x) x
#define TASMOTA_PLATFORM_MT7697N
#define USE_COUNTER
#include "include/tasmota.h"
#include "language/en_GB.h"
#include "include/tasmota_template.h"
#include "include/tasmota_types.h"
#include "../platform/settings.h"

constexpr uint16_t CFG_HOLDER = 4617;
TSettings actual_settings{};
TSettings* Settings = &actual_settings;
struct { bool stop_flash_rotate = false; } TasmotaGlobal;
uint32_t settings_location = 0, settings_crc32 = 0;
bool mt7697_settings_writable = false;
bool counters_disabled = false;
int saves = 0, defaults = 0, errors = 0;
mt7697::StorageResult load_result = mt7697::StorageResult::Missing;
mt7697::StorageResult save_result = mt7697::StorageResult::Ok;
TSettings stored{};

void AddLog(int level, const char*, ...) { if (level == LOG_LEVEL_ERROR) { ++errors; } }
void XsnsXdrvCall(int) {}
void UpdateBackwardCompatibility() {}
uint32_t UtcTime() { return START_VALID_TIME + 1; }
void RtcSettingsSave() {}
void RtcSettingsLoad(int) {}
void CounterInterruptDisable(bool disabled) { counters_disabled = disabled; }
void SettingsDefault();

namespace mt7697 {
StorageResult sdk_load_settings(void* data, size_t size) {
  assert(size == sizeof(stored));
  if (load_result == StorageResult::Ok) { memcpy(data, &stored, size); }
  return load_result;
}
StorageResult sdk_save_settings(const void* data, size_t size) {
  assert(size == sizeof(stored));
  ++saves;
  if (save_result == StorageResult::Ok) { memcpy(&stored, data, size); }
  return save_result;
}
}

#include "core_settings_functions.inc"

void SettingsDefault() {
  ++defaults;
  *Settings = TSettings{};
  Settings->cfg_holder = CFG_HOLDER;
  Settings->version = 0x0f000000;
  SettingsSave(2);  // The actual default path also requests persistence.
}

int main() {
  SettingsLoad();
  assert(defaults == 1 && saves == 1 && mt7697_settings_writable);
  assert(settings_location == 1 && stored.cfg_size == 4096);
  assert(settings_crc32 == stored.cfg_crc32);
  auto saved_crc = settings_crc32;
  Settings->save_data++;
  save_result = mt7697::StorageResult::NoSpace;
  SettingsSave(0);
  assert(saves == 2 && settings_crc32 == saved_crc);
  assert(!counters_disabled && errors == 1);
  save_result = mt7697::StorageResult::Ok;
  SettingsSave(0);
  assert(saves == 3 && settings_crc32 != saved_crc);
  SettingsSave(0);
  assert(saves == 3);  // Verified save is no longer dirty.

  load_result = mt7697::StorageResult::Ok;
  *Settings = TSettings{};
  SettingsLoad();
  assert(defaults == 1 && settings_location == 1);
  assert(Settings->save_data == stored.save_data);

  // Backend read failures must not let the default initialization overwrite flash.
  for (auto status : {mt7697::StorageResult::IoError, mt7697::StorageResult::Corrupt}) {
    load_result = status;
    const int previous_saves = saves;
    SettingsLoad();
    assert(!mt7697_settings_writable && saves == previous_saves);
    Settings->save_data++;
    SettingsSave(0);
    assert(saves == previous_saves);
  }
  // Bank CRC can be valid while Tasmota's internal settings CRC is not.
  load_result = mt7697::StorageResult::Ok;
  stored.cfg_crc32 ^= 1;
  const int previous_saves = saves;
  SettingsLoad();
  assert(!mt7697_settings_writable && saves == previous_saves);
  assert(!counters_disabled);
  puts("Core settings tests passed: native load/save, retry after failure, read-error preservation, core CRC.");
}
