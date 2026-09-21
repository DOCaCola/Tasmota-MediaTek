// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <stdint.h>

namespace mt7697 {
enum class NetworkState { Disabled, Connecting, Online, RetryWait, Fault };
enum class NetworkError { None, InvalidCredentials, StartFailed, StopFailed, TimedOut };

// SDK implementation and host fault-injection tests share this boundary.
class NetworkDriver {
 public:
  virtual ~NetworkDriver() = default;
  virtual bool start(const char* ssid, const char* password) = 0;
  virtual bool stop() = 0;
  virtual bool online() = 0;  // Requires association AND an IP address.
};

class Network {
 public:
  explicit Network(NetworkDriver& driver) : driver_(driver) {}
  // Main-task API. Invalid credentials leave an existing connection intact.
  bool begin(const char* ssid, const char* password, uint32_t now);
  bool stop();
  void poll(uint32_t now);
  NetworkState state() const { return state_; }
  NetworkError error() const { return error_; }

 private:
  void start(uint32_t now);
  void retry(uint32_t now, NetworkError error);
  NetworkDriver& driver_;
  NetworkState state_ = NetworkState::Disabled;
  NetworkError error_ = NetworkError::None;
  char ssid_[33] = {};
  char password_[64] = {};
  uint32_t since_ = 0;
  uint32_t retry_ms_ = 5000;
};
Network& station();  // Single native station owned by Tasmota's main task.
}  // namespace mt7697
