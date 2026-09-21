// SPDX-License-Identifier: GPL-3.0-or-later
#include "../platform/banked_settings.h"
#include <array>
#include <cassert>
#include <cstring>
#include <map>
#include <string>
#include <vector>
#include <cstdio>

using namespace mt7697;
struct Items : ItemStore {
  std::map<std::string, std::vector<uint8_t>> data;
  int writes = 0, fail_at = -1;
  bool tear = false;
  std::string read_error;
  StorageResult read(const char* key, uint8_t* output, size_t& size) override {
    if (read_error == key) { return StorageResult::IoError; }
    auto item = data.find(key);
    if (item == data.end()) { return StorageResult::Missing; }
    if (size < item->second.size()) { return StorageResult::InvalidSize; }
    size = item->second.size();
    memcpy(output, item->second.data(), size);
    return StorageResult::Ok;
  }
  StorageResult write(const char* key, const uint8_t* input, size_t size) override {
    assert(size <= 2048);
    if (++writes == fail_at) {
      if (tear) { data[key] = std::vector<uint8_t>(input, input + size / 2); }
      return StorageResult::IoError;
    }
    data[key] = std::vector<uint8_t>(input, input + size);
    return StorageResult::Ok;
  }
};
int main() {
  std::array<uint8_t, kSettingsSize> first, second, third, output;
  first.fill(0x11); second.fill(0x22); third.fill(0x33);
  Items initial;
  BankedSettings bank(initial);
  assert(load_settings(bank, output.data(), output.size()) == StorageResult::Missing);
  assert(save_settings(bank, first.data(), first.size()) == StorageResult::Ok);
  const auto one_saved = initial;
  // Interrupt every write boundary, with and without a partial item replacement.
  for (bool tear : {false, true}) {
    for (int failed = 1; failed <= 3; ++failed) {
      auto items = one_saved;
      items.fail_at = items.writes + failed;
      items.tear = tear;
      BankedSettings interrupted(items);
      assert(save_settings(interrupted, second.data(), second.size()) == StorageResult::IoError);
      BankedSettings rebooted(items);
      assert(load_settings(rebooted, output.data(), output.size()) == StorageResult::Ok);
      assert(output == first);
    }
  }
  assert(save_settings(bank, second.data(), second.size()) == StorageResult::Ok);
  assert(load_settings(bank, output.data(), output.size()) == StorageResult::Ok);
  assert(output == second);
  const auto two_saved = initial;
  // Unknown state of one bank must not be hidden by silently picking the other.
  initial.read_error = "BC";
  assert(load_settings(bank, output.data(), output.size()) == StorageResult::IoError);
  initial.read_error.clear();
  // Generation rollover: zero follows UINT32_MAX.
  auto wrapping = one_saved;
  uint32_t generation = UINT32_MAX;
  memcpy(wrapping.data["AC"].data() + 8, &generation, sizeof(generation));
  BankedSettings wrap_bank(wrapping);
  assert(save_settings(wrap_bank, second.data(), second.size()) == StorageResult::Ok);
  assert(load_settings(wrap_bank, output.data(), output.size()) == StorageResult::Ok);
  assert(output == second);
  // Reusing A with an old commit must not select a torn new payload.
  for (int failed = 1; failed <= 3; ++failed) {
    auto items = two_saved;
    items.fail_at = items.writes + failed;
    items.tear = true;
    BankedSettings interrupted(items);
    assert(save_settings(interrupted, third.data(), third.size()) == StorageResult::IoError);
    assert(load_settings(interrupted, output.data(), output.size()) == StorageResult::Ok);
    assert(output == second);
  }
  // Corrupt committed payload is rejected; surviving bank remains readable.
  initial.data["B1"][100] ^= 1;
  assert(load_settings(bank, output.data(), output.size()) == StorageResult::Ok);
  assert(output == first);
  initial.data["A0"][0] ^= 1;
  output.fill(0x77);
  auto untouched = output;
  assert(load_settings(bank, output.data(), output.size()) == StorageResult::Corrupt);
  assert(output == untouched);
  const auto write_count = initial.writes;
  assert(save_settings(bank, third.data(), third.size()) == StorageResult::Corrupt);
  assert(initial.writes == write_count);  // Never silently overwrite two corrupt banks.
  puts("Bank tests passed: 2048-byte limit, torn writes, bank reuse, CRC rejection and recovery.");
}
