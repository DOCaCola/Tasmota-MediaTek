// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <stdint.h>
#include <stddef.h>

#define MT7697_IMAGE_ID "TASMOTA:YLXD01YL:MT7697N:SDK1"
namespace mt7697 {
constexpr char kOtaImageId[32] = MT7697_IMAGE_ID;
constexpr uint32_t kOtaBase = 0x266000;
constexpr uint32_t kOtaLength = 0x18a000;
constexpr uint32_t kOtaSector = 4096;
constexpr uint32_t kOtaCapacity = kOtaLength - kOtaSector;
constexpr uint32_t kOtaHeader = 156;
constexpr uint32_t kApplicationBase = 0x79000;
constexpr uint32_t kApplicationLength = 0x1ed000;

enum class OtaResult {
  Ok, InvalidState, InvalidPackage, WrongImage, LayoutMismatch,
  FlashError, Incomplete, ChecksumMismatch, ActivationFailed
};
enum class OtaState { Idle, Receiving, Ready, Activated, Failed };

// All offsets are relative to the staging region. The final sector is reserved
// for bootloader trigger/status markers and never receives package data.
class OtaFlash {
 public:
  virtual ~OtaFlash() = default;
  virtual bool layout_matches() = 0;
  virtual bool read(uint32_t offset, void* bytes, size_t size) = 0;
  virtual bool erase_sector(uint32_t offset) = 0;
  virtual bool write(uint32_t offset, const void* bytes, size_t size) = 0;
  virtual bool activate() = 0;
};

bool ota_header_valid(const uint8_t* header, uint32_t package_size);

class OtaStager {
 public:
  explicit OtaStager(OtaFlash& flash) : flash_(flash) {}
  OtaResult begin(const uint8_t* header, uint32_t package_size);
  OtaResult append(const void* bytes, size_t size);
  OtaResult finish();
  OtaResult activate();
  OtaState state() const { return state_; }
  uint32_t received() const { return offset_; }
 private:
  OtaResult fail(OtaResult error) { state_ = OtaState::Failed; return error; }
  bool clear_marker();
  OtaFlash& flash_;
  OtaState state_ = OtaState::Idle;
  uint32_t size_ = 0, offset_ = 0, erased_ = 0;
  uint8_t header_[kOtaHeader] = {};
};
OtaFlash& sdk_ota_flash();
}  // namespace mt7697
