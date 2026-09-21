// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <stddef.h>
#include <stdint.h>

namespace mt7697 {
enum class StorageResult { Ok, Missing, Corrupt, NoSpace, InvalidSize, IoError, NoMemory };
constexpr size_t kSettingsSize = 4096;  // Current native TSettings size.

class SettingsBackend {
 public:
  virtual ~SettingsBackend() = default;
  virtual StorageResult read(uint8_t* data, size_t& size) = 0;
  virtual StorageResult write(const uint8_t* data, size_t size) = 0;
};

// Retains the caller's buffer on read failure and verifies each completed write.
// Tasmota remains responsible for its internal cfg_holder/version/CRC validation.
StorageResult load_settings(SettingsBackend& backend, void* data, size_t size);
StorageResult save_settings(SettingsBackend& backend, const void* data, size_t size);

// These functions use NVDM only in the coherent SDK layout, from the main task.
StorageResult sdk_load_settings(void* data, size_t size);
StorageResult sdk_save_settings(const void* data, size_t size);
}  // namespace mt7697
