// SPDX-License-Identifier: GPL-3.0-or-later
#include "banked_settings.h"
#include <stdlib.h>
#include <string.h>

namespace mt7697 {
namespace {
constexpr size_t kChunk = 2048;
constexpr uint32_t kMagic = 0x3153544d;  // "MTS1", little-endian format version 1.
struct Commit { uint32_t magic, size, generation, crc; };
static_assert(sizeof(Commit) == 16, "Commit format");
const char* const keys[2][3] = {{"A0", "A1", "AC"}, {"B0", "B1", "BC"}};

uint32_t crc32(const uint8_t* data, size_t size) {
  uint32_t crc = 0xffffffff;
  for (size_t i = 0; i < size; ++i) {
    crc ^= data[i];
    for (unsigned bit = 0; bit < 8; ++bit) {
      crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320u : 0u);
    }
  }
  return ~crc;
}
bool recoverable(StorageResult result) {
  return result == StorageResult::Missing || result == StorageResult::Corrupt ||
         result == StorageResult::InvalidSize;
}
}

StorageResult BankedSettings::read_bank(unsigned bank, uint8_t* data, uint32_t& generation) {
  Commit commit{};
  size_t size = sizeof(commit);
  auto result = store_.read(keys[bank][2], reinterpret_cast<uint8_t*>(&commit), size);
  if (result != StorageResult::Ok) { return result; }
  if (size != sizeof(commit) || commit.magic != kMagic || commit.size != kSettingsSize) {
    return StorageResult::Corrupt;
  }
  for (unsigned chunk = 0; chunk < 2; ++chunk) {
    size = kChunk;
    result = store_.read(keys[bank][chunk], data + chunk * kChunk, size);
    if (result != StorageResult::Ok) {
      return recoverable(result) ? StorageResult::Corrupt : result;
    }
    if (size != kChunk) { return StorageResult::Corrupt; }
  }
  if (crc32(data, kSettingsSize) != commit.crc) { return StorageResult::Corrupt; }
  generation = commit.generation;
  return StorageResult::Ok;
}

StorageResult BankedSettings::select(uint8_t* data, unsigned& bank, uint32_t& generation) {
  auto* candidate = static_cast<uint8_t*>(malloc(kSettingsSize));
  if (!candidate) { return StorageResult::NoMemory; }
  const auto first = read_bank(0, data, generation);
  uint32_t next_generation = 0;
  const auto second = read_bank(1, candidate, next_generation);
  StorageResult result;
  if (first != StorageResult::Ok && !recoverable(first)) { result = first; }
  else if (second != StorageResult::Ok && !recoverable(second)) { result = second; }
  else if (first == StorageResult::Ok || second == StorageResult::Ok) {
    bank = 0;
    if (second == StorageResult::Ok &&
        (first != StorageResult::Ok ||
         (next_generation != generation && uint32_t(next_generation - generation) < 0x80000000u))) {
      memcpy(data, candidate, kSettingsSize);
      generation = next_generation;
      bank = 1;
    }
    result = StorageResult::Ok;
  } else {
    result = first == StorageResult::Missing && second == StorageResult::Missing
        ? StorageResult::Missing : StorageResult::Corrupt;
  }
  free(candidate);
  return result;
}

StorageResult BankedSettings::read(uint8_t* data, size_t& size) {
  if (size != kSettingsSize) { return StorageResult::InvalidSize; }
  unsigned bank = 0;
  uint32_t generation = 0;
  return select(data, bank, generation);
}

StorageResult BankedSettings::write(const uint8_t* data, size_t size) {
  if (size != kSettingsSize) { return StorageResult::InvalidSize; }
  auto* scratch = static_cast<uint8_t*>(malloc(kSettingsSize));
  if (!scratch) { return StorageResult::NoMemory; }
  unsigned bank = 1;  // With no committed bank, first save targets A.
  uint32_t generation = 0;
  auto result = select(scratch, bank, generation);
  if (result != StorageResult::Ok && result != StorageResult::Missing) {
    free(scratch);
    return result;
  }
  bank ^= 1;
  // Verify all chunks before writing the commit record. An old commit record
  // in the target bank cannot validate partially replaced data because of CRC.
  for (unsigned chunk = 0; chunk < 2; ++chunk) {
    result = store_.write(keys[bank][chunk], data + chunk * kChunk, kChunk);
    if (result != StorageResult::Ok) { break; }
    size_t actual = kChunk;
    result = store_.read(keys[bank][chunk], scratch, actual);
    if (result != StorageResult::Ok) { break; }
    if (actual != kChunk || memcmp(scratch, data + chunk * kChunk, kChunk) != 0) {
      result = StorageResult::Corrupt;
      break;
    }
  }
  if (result == StorageResult::Ok) {
    const Commit commit{kMagic, kSettingsSize, generation + 1, crc32(data, size)};
    result = store_.write(keys[bank][2], reinterpret_cast<const uint8_t*>(&commit), sizeof(commit));
  }
  free(scratch);
  return result;
}
}
