// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "settings.h"

namespace mt7697 {
class ItemStore {
 public:
  virtual ~ItemStore() = default;
  virtual StorageResult read(const char* key, uint8_t* data, size_t& size) = 0;
  virtual StorageResult write(const char* key, const uint8_t* data, size_t size) = 0;
};

// Two 2048-byte chunks per bank, plus a commit record. All calls are serialized
// by the application main task. The previous bank is never changed by a save.
class BankedSettings final : public SettingsBackend {
 public:
  explicit BankedSettings(ItemStore& store) : store_(store) {}
  StorageResult read(uint8_t* data, size_t& size) override;
  StorageResult write(const uint8_t* data, size_t size) override;
 private:
  StorageResult read_bank(unsigned bank, uint8_t* data, uint32_t& generation);
  StorageResult select(uint8_t* data, unsigned& bank, uint32_t& generation);
  ItemStore& store_;
};
}
