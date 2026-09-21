// SPDX-License-Identifier: GPL-3.0-or-later
#include "settings.h"
#include <stdlib.h>
#include <string.h>

namespace mt7697 {
StorageResult load_settings(SettingsBackend& backend, void* data, size_t size) {
  if (size != kSettingsSize) { return StorageResult::InvalidSize; }
  auto* scratch = static_cast<uint8_t*>(malloc(size));
  if (!scratch) { return StorageResult::NoMemory; }
  size_t actual = size;
  StorageResult result = backend.read(scratch, actual);
  if (result == StorageResult::Ok && actual != size) { result = StorageResult::InvalidSize; }
  if (result == StorageResult::Ok) { memcpy(data, scratch, size); }
  free(scratch);
  return result;
}

StorageResult save_settings(SettingsBackend& backend, const void* data, size_t size) {
  if (size != kSettingsSize) { return StorageResult::InvalidSize; }
  // Allocate verification storage before changing flash.
  auto* scratch = static_cast<uint8_t*>(malloc(size));
  if (!scratch) { return StorageResult::NoMemory; }
  StorageResult result = backend.write(static_cast<const uint8_t*>(data), size);
  if (result == StorageResult::Ok) {
    size_t actual = size;
    result = backend.read(scratch, actual);
    if (result == StorageResult::Ok &&
        (actual != size || memcmp(data, scratch, size) != 0)) {
      result = StorageResult::Corrupt;
    }
  }
  free(scratch);
  return result;
}
}  // namespace mt7697
