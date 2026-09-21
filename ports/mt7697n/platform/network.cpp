// SPDX-License-Identifier: GPL-3.0-or-later
#include "network.h"
#include <string.h>

namespace mt7697 {
bool Network::begin(const char* ssid, const char* password, uint32_t now) {
  const size_t ssid_size = strlen(ssid);
  const size_t password_size = strlen(password);
  bool valid = ssid_size > 0 && ssid_size < sizeof(ssid_) &&
               (password_size == 0 || (password_size >= 8 && password_size < sizeof(password_)));
  for (size_t i = 0; i < password_size; ++i) {
    valid = valid && static_cast<unsigned char>(password[i]) >= 32 &&
            static_cast<unsigned char>(password[i]) <= 126;
  }
  if (!valid) {
    error_ = NetworkError::InvalidCredentials;
    return false;
  }
  if (state_ != NetworkState::Disabled && !stop()) { return false; }
  memcpy(ssid_, ssid, ssid_size + 1);
  memcpy(password_, password, password_size + 1);
  retry_ms_ = 5000;
  start(now);
  return state_ == NetworkState::Connecting;
}

bool Network::stop() {
  if (state_ != NetworkState::Disabled && !driver_.stop()) {
    state_ = NetworkState::Fault;
    error_ = NetworkError::StopFailed;
    return false;
  }
  state_ = NetworkState::Disabled;
  error_ = NetworkError::None;
  memset(ssid_, 0, sizeof(ssid_));
  memset(password_, 0, sizeof(password_));
  return true;
}

void Network::start(uint32_t now) {
  if (!driver_.start(ssid_, password_)) {
    // Failed configuration may have partially started the driver.
    if (!driver_.stop()) {
      state_ = NetworkState::Fault;
      error_ = NetworkError::StopFailed;
      return;
    }
    retry(now, NetworkError::StartFailed);
    return;
  }
  since_ = now;
  state_ = NetworkState::Connecting;
  error_ = NetworkError::None;
}

void Network::retry(uint32_t now, NetworkError error) {
  state_ = NetworkState::RetryWait;
  since_ = now;
  error_ = error;
}

void Network::poll(uint32_t now) {
  if (state_ == NetworkState::Disabled || state_ == NetworkState::Fault) { return; }
  if (state_ == NetworkState::RetryWait) {
    if (static_cast<uint32_t>(now - since_) >= retry_ms_) {
      retry_ms_ = retry_ms_ >= 30000 ? 60000 : retry_ms_ * 2;
      start(now);
    }
    return;
  }
  if (driver_.online()) {
    state_ = NetworkState::Online;
    error_ = NetworkError::None;
    retry_ms_ = 5000;
    return;
  }
  const bool timed_out = state_ == NetworkState::Connecting &&
                        static_cast<uint32_t>(now - since_) >= 30000;
  if (state_ == NetworkState::Online || timed_out) {
    if (!driver_.stop()) {
      state_ = NetworkState::Fault;
      error_ = NetworkError::StopFailed;
      return;
    }
    retry(now, timed_out ? NetworkError::TimedOut : NetworkError::None);
  }
}
}  // namespace mt7697
