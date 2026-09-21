// SPDX-License-Identifier: GPL-3.0-or-later
#include "ota.h"
#include <string.h>
#include <t_bearssl_hash.h>

namespace mt7697 {
namespace {
uint32_t word(const uint8_t* p) {
  return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}
void sha1(const void* data, size_t size, uint8_t* digest) {
  br_sha1_context hash;
  br_sha1_init(&hash); br_sha1_update(&hash, data, size); br_sha1_out(&hash, digest);
}
}

bool ota_header_valid(const uint8_t* header, uint32_t size) {
  if (size < kOtaHeader + 32 + 20 || size > kOtaCapacity ||
      word(header) != 0x004d4d4d || word(header + 4) != 1) { return false; }
  const uint32_t length = size - kOtaHeader - 20;
  if ((length & 3) || length > kApplicationLength ||
      word(header + 8) != kOtaHeader || word(header + 12) != kApplicationBase ||
      word(header + 16) != length || word(header + 20) != kApplicationLength ||
      word(header + 24) != size - 20 || word(header + 28) != 20 ||
      word(header + 32) != 0) { return false; } // Only uncompressed application images.
  for (unsigned i = 36; i < 136; ++i) {
    if (header[i]) { return false; } // Reserved word and all unused descriptors.
  }
  uint8_t digest[20];
  sha1(header, 136, digest);
  return memcmp(digest, header + 136, sizeof(digest)) == 0;
}

bool OtaStager::clear_marker() {
  if (!flash_.erase_sector(kOtaCapacity)) { return false; }
  uint8_t block[256];
  for (uint32_t offset = kOtaCapacity; offset < kOtaLength; offset += sizeof(block)) {
    if (!flash_.read(offset, block, sizeof(block))) { return false; }
    for (uint8_t byte : block) { if (byte != 0xff) { return false; } }
  }
  return true;
}

OtaResult OtaStager::begin(const uint8_t* header, uint32_t size) {
  if (state_ != OtaState::Idle && state_ != OtaState::Failed) { return OtaResult::InvalidState; }
  if (!ota_header_valid(header, size)) { return fail(OtaResult::InvalidPackage); }
  if (!flash_.layout_matches()) { return fail(OtaResult::LayoutMismatch); }
  // Invalidate previous trigger AND interrupted-update marker before replacing
  // any package bytes. Power loss while receiving leaves the active image alone.
  if (!clear_marker()) { return fail(OtaResult::FlashError); }
  size_ = size;
  offset_ = erased_ = 0;
  memcpy(header_, header, sizeof(header_));
  state_ = OtaState::Receiving;
  return append(header_, sizeof(header_));
}

OtaResult OtaStager::append(const void* bytes, size_t size) {
  if (state_ != OtaState::Receiving) { return OtaResult::InvalidState; }
  if (!size || size > size_ - offset_) { return fail(OtaResult::InvalidPackage); }
  const uint32_t end = offset_ + size;
  while (erased_ < end) {
    if (!flash_.erase_sector(erased_)) { return fail(OtaResult::FlashError); }
    erased_ += kOtaSector;
  }
  if (!flash_.write(offset_, bytes, size)) { return fail(OtaResult::FlashError); }
  offset_ = end;
  return OtaResult::Ok;
}

OtaResult OtaStager::finish() {
  if (state_ != OtaState::Receiving) { return OtaResult::InvalidState; }
  if (offset_ != size_) { return fail(OtaResult::Incomplete); }
  uint8_t block[1024 + sizeof(kOtaImageId) - 1];
  if (!flash_.read(0, block, kOtaHeader)) { return fail(OtaResult::FlashError); }
  if (memcmp(header_, block, kOtaHeader)) { return fail(OtaResult::ChecksumMismatch); }
  br_sha1_context hash;
  br_sha1_init(&hash);
  bool identity_found = false;
  size_t tail = 0;
  const uint32_t payload_end = size_ - 20;
  for (uint32_t pos = kOtaHeader; pos < payload_end;) {
    const uint32_t length = payload_end - pos > 1024 ? 1024 : payload_end - pos;
    if (!flash_.read(pos, block + tail, length)) { return fail(OtaResult::FlashError); }
    br_sha1_update(&hash, block + tail, length);
    for (size_t i = 0; i + sizeof(kOtaImageId) <= tail + length; ++i) {
      if (!memcmp(block + i, kOtaImageId, sizeof(kOtaImageId))) { identity_found = true; }
    }
    const size_t available = tail + length;
    tail = available < sizeof(kOtaImageId) - 1 ? available : sizeof(kOtaImageId) - 1;
    memmove(block, block + available - tail, tail);
    pos += length;
  }
  uint8_t digest[20];
  br_sha1_out(&hash, digest);
  if (!flash_.read(payload_end, block, sizeof(digest))) { return fail(OtaResult::FlashError); }
  if (memcmp(block, digest, sizeof(digest))) { return fail(OtaResult::ChecksumMismatch); }
  if (!identity_found) { return fail(OtaResult::WrongImage); }
  state_ = OtaState::Ready;
  return OtaResult::Ok;
}

OtaResult OtaStager::activate() {
  if (state_ != OtaState::Ready) { return OtaResult::InvalidState; }
  uint8_t marker[4];
  if (!flash_.activate() || !flash_.read(kOtaLength - 512, marker, sizeof(marker)) ||
      word(marker) != 0x004d4d4d) {
    const bool cleared = clear_marker();
    return fail(cleared ? OtaResult::ActivationFailed : OtaResult::FlashError);
  }
  state_ = OtaState::Activated;
  return OtaResult::Ok; // Caller decides when to reboot; staging never reboots.
}
}  // namespace mt7697
