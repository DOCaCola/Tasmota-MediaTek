// SPDX-License-Identifier: GPL-3.0-or-later
#include <platform/ota.h>
#include <t_bearssl_hash.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <vector>
#include <fstream>
using namespace mt7697;

struct Flash : OtaFlash {
  std::vector<uint8_t> data = std::vector<uint8_t>(kOtaLength, 0x55);
  bool layout = true, bad_activation = false, bad_marker = false;
  unsigned operations = 0, fail_at = 0, erases = 0, writes = 0, activations = 0;
  bool operation() { return ++operations != fail_at; }
  bool layout_matches() override { return layout; }
  bool read(uint32_t offset, void* output, size_t size) override {
    assert(offset <= data.size() && size <= data.size() - offset);
    if (!operation()) return false;
    memcpy(output, data.data() + offset, size); return true;
  }
  bool erase_sector(uint32_t offset) override {
    assert(offset % 4096 == 0 && offset < kOtaLength);
    ++erases;
    if (!operation()) return false;
    memset(data.data() + offset, 0xff, 4096); return true;
  }
  bool write(uint32_t offset, const void* input, size_t size) override {
    assert(offset <= kOtaCapacity && size <= kOtaCapacity - offset);
    ++writes;
    if (!operation()) return false;
    auto bytes = static_cast<const uint8_t*>(input);
    for (size_t i = 0; i < size; ++i) {
      assert((data[offset + i] & bytes[i]) == bytes[i]);
      data[offset + i] &= bytes[i];
    }
    return true;
  }
  bool activate() override {
    ++activations;
    if (!operation()) return false;
    if (!bad_marker) memcpy(data.data() + kOtaLength - 512, "MMM\0", 4);
    return !bad_activation;
  }
  bool unarmed() const {
    return data[kOtaLength - 512] == 0xff && data[kOtaLength - 496] == 0xff;
  }
};

static void hash(const uint8_t* in, size_t size, uint8_t* out) {
  br_sha1_context ctx; br_sha1_init(&ctx); br_sha1_update(&ctx, in, size); br_sha1_out(&ctx, out);
}
static void word(std::vector<uint8_t>& p, size_t offset, uint32_t value) {
  for (unsigned i = 0; i < 4; ++i) p[offset + i] = value >> (8*i);
}
static OtaResult transfer(OtaStager& stage, const std::vector<uint8_t>& package) {
  auto status = stage.begin(package.data(), package.size());
  if (status != OtaResult::Ok) return status;
  for (size_t offset = kOtaHeader; offset < package.size();) {
    const size_t size = package.size() - offset > 701 ? 701 : package.size() - offset;
    status = stage.append(package.data() + offset, size);
    if (status != OtaResult::Ok) return status;
    offset += size;
  }
  return stage.finish();
}
int main(int argc, char** argv) {
  assert(argc == 2);
  std::ifstream input(argv[1], std::ios::binary);
  const std::vector<uint8_t> package((std::istreambuf_iterator<char>(input)), {});
  assert(!package.empty() && ota_header_valid(package.data(), package.size()));
  // Fixture is made by Python hashlib, independently checking BearSSL digests.
  Flash flash; OtaStager stage(flash);
  assert(stage.activate() == OtaResult::InvalidState);
  assert(transfer(stage, package) == OtaResult::Ok);
  assert(flash.unarmed() && flash.activations == 0);
  assert(stage.activate() == OtaResult::Ok && stage.state() == OtaState::Activated);
  assert(stage.activate() == OtaResult::InvalidState);

  Flash wrong_layout; wrong_layout.layout = false;
  OtaStager rejected(wrong_layout);
  assert(transfer(rejected, package) == OtaResult::LayoutMismatch);
  assert(wrong_layout.operations == 0);
  // Mutate each field with a RECOMPUTED header hash: rejection must be semantic.
  for (auto offset : {0,4,8,12,16,20,24,28,32,36,40,132}) {
    auto bad = package; word(bad, offset, 0x12345678);
    hash(bad.data(), 136, bad.data() + 136);
    Flash target; OtaStager reject(target);
    assert(transfer(reject, bad) == OtaResult::InvalidPackage && target.operations == 0);
  }
  for (auto offset : {136U, 155U}) {
    auto bad = package; bad[offset] ^= 1;
    assert(!ota_header_valid(bad.data(), bad.size()));
  }
  assert(!ota_header_valid(package.data(), 0));
  assert(!ota_header_valid(package.data(), UINT32_MAX));
  assert(!ota_header_valid(package.data(), kOtaCapacity + 1));
  Flash partial; OtaStager incomplete(partial);
  assert(incomplete.begin(package.data(), package.size()) == OtaResult::Ok);
  assert(incomplete.finish() == OtaResult::Incomplete && partial.unarmed());
  assert(incomplete.activate() == OtaResult::InvalidState);
  Flash overflow_flash; OtaStager overflow(overflow_flash);
  assert(overflow.begin(package.data(), package.size()) == OtaResult::Ok);
  assert(overflow.begin(package.data(), package.size()) == OtaResult::InvalidState);
  assert(overflow.append(package.data(), package.size()) == OtaResult::InvalidPackage);
  assert(overflow_flash.unarmed() && overflow.activate() == OtaResult::InvalidState);
  // A failed transfer can be replaced; an already verified package cannot.
  assert(transfer(overflow, package) == OtaResult::Ok);
  assert(overflow.begin(package.data(), package.size()) == OtaResult::InvalidState);

  // Each flash failure must terminate before activation. Covers trigger
  // invalidation, sector erase, write and readback verification.
  const unsigned transfer_ops = flash.operations - 2; // Exclude activation + marker read.
  for (unsigned fault = 1; fault <= transfer_ops; ++fault) {
    Flash target; target.fail_at = fault; OtaStager failed(target);
    assert(transfer(failed, package) == OtaResult::FlashError);
    assert(failed.state() == OtaState::Failed && !target.activations);
    assert(failed.activate() == OtaResult::InvalidState);
  }
  for (unsigned corruption : {200U, unsigned(package.size() - 1)}) {
    auto bad = package; bad[corruption] ^= 1;
    Flash target; OtaStager failed(target);
    assert(transfer(failed, bad) == OtaResult::ChecksumMismatch && target.unarmed());
  }
  auto foreign = package;
  memset(foreign.data() + kOtaHeader, 0, foreign.size() - kOtaHeader - 20);
  hash(foreign.data() + kOtaHeader, foreign.size() - kOtaHeader - 20, foreign.data() + foreign.size() - 20);
  Flash foreign_flash; OtaStager foreign_stage(foreign_flash);
  assert(transfer(foreign_stage, foreign) == OtaResult::WrongImage && foreign_flash.unarmed());
  for (unsigned mode = 0; mode < 3; ++mode) {
    Flash target; OtaStager failed(target);
    assert(transfer(failed, package) == OtaResult::Ok);
    if (mode == 0) target.bad_activation = true; // Flag written but API reports failure.
    if (mode == 1) target.bad_marker = true;     // API success without a written flag.
    if (mode == 2) target.fail_at = target.operations + 2; // Marker readback fails.
    assert(failed.activate() == OtaResult::ActivationFailed);
    assert(target.unarmed() && failed.state() == OtaState::Failed);
  }
  Flash cleanup_flash; OtaStager cleanup(cleanup_flash);
  assert(transfer(cleanup, package) == OtaResult::Ok);
  cleanup_flash.bad_activation = true;
  cleanup_flash.fail_at = cleanup_flash.operations + 2; // Cleanup erase fails.
  assert(cleanup.activate() == OtaResult::FlashError);
  // A failed cleanup cannot promise the trigger is clear. Report a hard error.
  assert(!cleanup_flash.unarmed() && cleanup.state() == OtaState::Failed);
  puts("OTA tests passed: canonical header, layout gate, write/read faults, identity, activation cleanup");
}
