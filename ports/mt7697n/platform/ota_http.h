// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "ota.h"

namespace mt7697 {
struct OtaUrl {
  uint8_t ip[4];
  uint16_t port;
  char host[22], path[256];
};
bool parse_ota_url(const char* text, OtaUrl& url);

// Nonblocking transport: connect returns success/failure, connected and I/O
// return -1 on error, 0 when pending, positive on progress.
class OtaTransport {
 public:
  virtual ~OtaTransport() = default;
  virtual bool connect(const OtaUrl& url) = 0;
  virtual int connected() = 0;
  virtual int send(const uint8_t* data, size_t size) = 0;
  virtual int receive(uint8_t* data, size_t size) = 0;
  virtual void close() = 0;
};
enum class OtaDownloadState { Idle, Connecting, Sending, Headers, Body, Ready, Failed };
class OtaDownload {
 public:
  OtaDownload(OtaTransport& transport, OtaStager& stager) : io_(transport), stage_(stager) {}
  bool start(const char* url, uint32_t now);
  void poll(uint32_t now, bool online);
  bool busy() const {
    return state_ >= OtaDownloadState::Connecting && state_ <= OtaDownloadState::Body;
  }
  OtaDownloadState state() const { return state_; }
  const char* error() const { return error_; }
  OtaResult staging_result() const { return result_; }
 private:
  void fail(const char* reason);
  bool header_line();
  OtaTransport& io_;
  OtaStager& stage_;
  OtaDownloadState state_ = OtaDownloadState::Idle;
  const char* error_ = "";
  OtaResult result_ = OtaResult::Ok;
  uint32_t started_ = 0, progress_ = 0, length_ = 0, received_ = 0;
  size_t request_size_ = 0, sent_ = 0, line_size_ = 0, header_bytes_ = 0;
  bool status_seen_ = false, length_seen_ = false;
  char request_[384], line_[256];
  uint8_t header_[kOtaHeader];
};
OtaTransport& sdk_ota_transport();
}  // namespace mt7697
