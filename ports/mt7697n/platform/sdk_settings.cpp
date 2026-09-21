// SPDX-License-Identifier: GPL-3.0-or-later
#include "settings.h"
#include "banked_settings.h"
extern "C" {
#include <nvdm.h>
#include <flash_map.h>
}

static_assert(NVDM_BASE == 0x3f0000 && NVDM_LENGTH == 0x10000,
              "NVDM backend requires the coherent SDK flash layout");

namespace mt7697 {
namespace {
StorageResult translate(nvdm_status_t status) {
  switch (status) {
    case NVDM_STATUS_OK: return StorageResult::Ok;
    case NVDM_STATUS_ITEM_NOT_FOUND: return StorageResult::Missing;
    case NVDM_STATUS_INCORRECT_CHECKSUM: return StorageResult::Corrupt;
    case NVDM_STATUS_INSUFFICIENT_SPACE: return StorageResult::NoSpace;
    default: return StorageResult::IoError;
  }
}
class NvdmItems final : public ItemStore {
 public:
  StorageResult initialize() {
    // Main-task ownership; NVDM initialization must not be repeated.
    if (!attempted_) {
      attempted_ = true;
      initialized_ = translate(nvdm_init());
    }
    return initialized_;
  }
  StorageResult read(const char* key, uint8_t* data, size_t& size) override {
    uint32_t actual = size;
    const auto result = translate(nvdm_read_data_item("Tasmota", key, data, &actual));
    size = actual;
    return result;
  }
  StorageResult write(const char* key, const uint8_t* data, size_t size) override {
    return translate(nvdm_write_data_item("Tasmota", key,
                                        NVDM_DATA_ITEM_TYPE_RAW_DATA, data, size));
  }
 private:
  bool attempted_ = false;
  StorageResult initialized_ = StorageResult::IoError;
};
NvdmItems items;
BankedSettings backend(items);
}
StorageResult sdk_load_settings(void* data, size_t size) {
  if (size != kSettingsSize) { return StorageResult::InvalidSize; }
  const auto status = items.initialize();
  return status == StorageResult::Ok ? load_settings(backend, data, size) : status;
}
StorageResult sdk_save_settings(const void* data, size_t size) {
  if (size != kSettingsSize) { return StorageResult::InvalidSize; }
  const auto status = items.initialize();
  return status == StorageResult::Ok ? save_settings(backend, data, size) : status;
}
}
