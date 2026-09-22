// SPDX-License-Identifier: GPL-3.0-or-later
#include "ota.h"
#include "sdk_layout.h"
#include <string.h>
#include <t_bearssl_hash.h>
extern "C" {
#include <FreeRTOS.h>
#include <task.h>
#include <hal_flash.h>
#include <flash_map.h>
#include <fota.h>
extern const uint8_t __FLASH_segment_start__;
}
static_assert(FOTA_BASE == mt7697::kOtaBase && FOTA_LENGTH == mt7697::kOtaLength,
              "OTA backend requires the pinned SDK layout");

namespace mt7697 {
namespace {
bool fingerprint(uint32_t address, uint32_t size, const uint8_t* expected) {
  br_sha1_context hash;
  br_sha1_init(&hash);
  uint8_t block[512], digest[20];
  while (size) {
    uint32_t length = size > sizeof(block) ? sizeof(block) : size;
    if (hal_flash_read(address, block, length) != HAL_FLASH_STATUS_OK) { return false; }
    br_sha1_update(&hash, block, length);
    address += length;
    size -= length;
  }
  br_sha1_out(&hash, digest);
  return !memcmp(digest, expected, sizeof(digest));
}

class SdkOtaFlash final : public OtaFlash {
 public:
  bool layout_matches() override {
    // GD25Q32-capable official loader and pinned BSP 0.10.21 radio.
    // Do not enable writes when running against the stock lamp flash layout.
    static const uint8_t radio[20] = {
      0xc3,0xf8,0x94,0x9f,0xb6,0x8f,0x44,0x64,0xdf,0xdb,0xe7,0xab,0x19,0x95,0x77,0xa2,0x80,0x07,0x06,0xa7};
    return uintptr_t(&__FLASH_segment_start__) == 0x10079000 &&
           fingerprint(0, kBootloaderBytes, kBootloaderSha1) &&
           fingerprint(0x10000, 297882, radio);
  }
  bool read(uint32_t offset, void* bytes, size_t size) override {
    return offset <= kOtaLength && size <= kOtaLength - offset &&
           hal_flash_read(kOtaBase + offset, static_cast<uint8_t*>(bytes), size) == HAL_FLASH_STATUS_OK;
  }
  bool erase_sector(uint32_t offset) override {
    if (offset >= kOtaLength || offset % kOtaSector) { return false; }
    taskENTER_CRITICAL();
    const auto result = hal_flash_erase(kOtaBase + offset, HAL_FLASH_BLOCK_4K);
    taskEXIT_CRITICAL();
    return result == HAL_FLASH_STATUS_OK;
  }
  bool write(uint32_t offset, const void* bytes, size_t size) override {
    if (offset > kOtaCapacity || size > kOtaCapacity - offset) { return false; }
    taskENTER_CRITICAL();
    const auto result = hal_flash_write(kOtaBase + offset, static_cast<const uint8_t*>(bytes), size);
    taskEXIT_CRITICAL();
    return result == HAL_FLASH_STATUS_OK;
  }
  bool activate() override {
    // This high-level SDK routine logs before and after writing the marker.
    // Its logger may block on a full queue: masking the scheduler here prevents
    // that queue from draining and can hang after the marker is committed.
    // The SDK flash driver owns the SFC lock and masks its hardware commands.
    const auto result = fota_trigger_update();
    return result == FOTA_TRIGGER_SUCCESS;
  }
};
SdkOtaFlash flash;
}
OtaFlash& sdk_ota_flash() { return flash; }
}

// Kept for link coverage; never called during startup or from a command.
extern "C" bool mt7697_ota_link_check(const uint8_t* header, uint32_t size) {
  mt7697::OtaStager stager(mt7697::sdk_ota_flash());
  return stager.begin(header, size) == mt7697::OtaResult::Ok &&
         stager.finish() == mt7697::OtaResult::Ok &&
         stager.activate() == mt7697::OtaResult::Ok;
}
