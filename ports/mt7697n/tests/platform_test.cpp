// SPDX-License-Identifier: GPL-3.0-or-later
#include "../platform/network.h"
#include "../platform/settings.h"
#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>

using namespace mt7697;
struct FakeNetwork : NetworkDriver {
  bool starts_ok = true, stops_ok = true, connected = false;
  int starts = 0, stops = 0;
  bool start(const char*, const char*) override { ++starts; return starts_ok; }
  bool stop() override { ++stops; connected = false; return stops_ok; }
  bool online() override { return connected; }
};
struct FakeStorage : SettingsBackend {
  std::array<uint8_t, kSettingsSize> saved{};
  StorageResult read_result = StorageResult::Missing, write_result = StorageResult::Ok;
  size_t read_size = kSettingsSize;
  bool corrupt_write = false;
  int writes = 0;
  StorageResult read(uint8_t* data, size_t& size) override {
    assert(size == kSettingsSize);
    memcpy(data, saved.data(), saved.size());
    size = read_size;
    return read_result;
  }
  StorageResult write(const uint8_t* data, size_t size) override {
    ++writes;
    if (write_result != StorageResult::Ok) { return write_result; }
    assert(size == saved.size());
    memcpy(saved.data(), data, size);
    if (corrupt_write) { saved[10] ^= 1; }
    read_result = StorageResult::Ok;
    return write_result;
  }
};

int main() {
  FakeNetwork driver;
  Network network(driver);
  assert(!network.begin("", "", 0));
  assert(!network.begin("ssid", "short", 0));
  assert(!network.begin("ssid", "password\n", 0));
  assert(driver.starts == 0);
  assert(network.begin("ssid", "password", 100));
  assert(network.state() == NetworkState::Connecting);
  network.poll(30099);
  assert(driver.stops == 0);
  network.poll(30100);
  assert(network.error() == NetworkError::TimedOut);
  assert(driver.stops == 1);
  network.poll(35099);
  assert(driver.starts == 1);
  network.poll(35100);
  assert(driver.starts == 2);
  driver.connected = true;
  network.poll(35101);
  assert(network.state() == NetworkState::Online);
  assert(!network.begin("ssid", "bad", 35102));
  assert(network.state() == NetworkState::Online && driver.connected);
  driver.connected = false;
  network.poll(35103);
  assert(network.state() == NetworkState::RetryWait);
  network.poll(40103);
  assert(driver.starts == 3);
  assert(network.stop());
  network.poll(1000000);
  assert(driver.starts == 3 && network.state() == NetworkState::Disabled);

  // Unsigned elapsed-time comparisons across millis() wrap.
  assert(network.begin("open", "", UINT32_MAX - 10000));
  network.poll(19998);
  assert(network.state() == NetworkState::Connecting);
  network.poll(19999);
  assert(network.state() == NetworkState::RetryWait);
  driver.starts_ok = false;
  network.poll(24999);
  assert(network.error() == NetworkError::StartFailed);
  int attempts = driver.starts;
  network.poll(34998);
  assert(driver.starts == attempts);
  network.poll(34999);
  assert(driver.starts == attempts + 1);
  // Stop failure is a fault, never silently "disabled".
  driver.stops_ok = false;
  assert(!network.stop());
  assert(network.state() == NetworkState::Fault);
  assert(network.error() == NetworkError::StopFailed);
  network.poll(999999);
  assert(driver.starts == attempts + 1);
  driver.stops_ok = true;
  assert(network.stop());

  FakeStorage storage;
  std::array<uint8_t, kSettingsSize> settings;
  settings.fill(0x5a);
  const auto original = settings;
  assert(load_settings(storage, settings.data(), settings.size()) == StorageResult::Missing);
  assert(settings == original);  // Even if a failed backend read scribbled into scratch.
  storage.read_result = StorageResult::Corrupt;
  assert(load_settings(storage, settings.data(), settings.size()) == StorageResult::Corrupt);
  assert(settings == original);
  storage.read_result = StorageResult::Ok;
  storage.read_size = 100;
  assert(load_settings(storage, settings.data(), settings.size()) == StorageResult::InvalidSize);
  assert(settings == original);
  storage.read_size = kSettingsSize;
  assert(save_settings(storage, settings.data(), settings.size()) == StorageResult::Ok);
  settings.fill(0);
  assert(load_settings(storage, settings.data(), settings.size()) == StorageResult::Ok);
  assert(settings == original);
  storage.write_result = StorageResult::NoSpace;
  assert(save_settings(storage, settings.data(), settings.size()) == StorageResult::NoSpace);
  storage.write_result = StorageResult::Ok;
  storage.corrupt_write = true;
  assert(save_settings(storage, settings.data(), settings.size()) == StorageResult::Corrupt);
  const int writes = storage.writes;
  assert(save_settings(storage, settings.data(), 12) == StorageResult::InvalidSize);
  assert(storage.writes == writes);
  puts("Platform tests passed: connection lifecycle, retry, timer wrap, failures, settings preservation and readback.");
}
