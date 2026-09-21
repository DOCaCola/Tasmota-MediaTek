// SPDX-License-Identifier: GPL-3.0-or-later
#include "ota_http.h"
#include <string.h>
#include <stdio.h>

namespace mt7697 {
bool parse_ota_url(const char* text, OtaUrl& url) {
  if (strncmp(text, "http://", 7)) return false;
  const char* p = text + 7;
  const char* host = p;
  for (unsigned i = 0; i < 4; ++i) {
    unsigned n = 0, digits = 0;
    while (*p >= '0' && *p <= '9') {
      n = n * 10 + *p++ - '0';
      if (++digits > 3 || n > 255) return false;
    }
    if (!digits) return false;
    url.ip[i] = n;
    if (i < 3 && *p++ != '.') return false;
  }
  // Unicast destination only.
  if (!url.ip[0] || url.ip[0] >= 224) return false;
  url.port = 80;
  if (*p == ':') {
    ++p;
    unsigned n = 0, digits = 0;
    while (*p >= '0' && *p <= '9') {
      n = n * 10 + *p++ - '0';
      if (++digits > 5 || n > 65535) return false;
    }
    if (!digits || !n) return false;
    url.port = n;
  }
  const size_t host_size = p - host;
  if (*p != '/' || host_size >= sizeof(url.host) || strlen(p) >= sizeof(url.path)) return false;
  for (const char* q = p; *q; ++q) {
    if (static_cast<unsigned char>(*q) <= 32 || static_cast<unsigned char>(*q) >= 127 || *q == '#') return false;
  }
  memcpy(url.host, host, host_size); url.host[host_size] = 0;
  strcpy(url.path, p);
  return true;
}

void OtaDownload::fail(const char* reason) {
  error_ = reason;
  state_ = OtaDownloadState::Failed;
  io_.close();
  stage_.abort();
}
bool OtaDownload::start(const char* text, uint32_t now) {
  if (busy() || state_ == OtaDownloadState::Ready) return false;
  OtaUrl url;
  if (!parse_ota_url(text, url)) { error_ = "Expected http://IPv4[:port]/path"; return false; }
  stage_.abort();
  io_.close();
  error_ = "";
  result_ = OtaResult::Ok;
  const int count = snprintf(request_, sizeof(request_),
    "GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\nAccept-Encoding: identity\r\n\r\n", url.path, url.host);
  if (count < 0 || size_t(count) >= sizeof(request_)) { fail("Request too long"); return false; }
  request_size_ = count;
  sent_ = line_size_ = header_bytes_ = 0;
  length_ = received_ = 0;
  status_seen_ = length_seen_ = false;
  started_ = progress_ = now;
  if (!io_.connect(url)) { fail("Connect failed"); return false; }
  state_ = OtaDownloadState::Connecting;
  return true;
}
bool OtaDownload::header_line() {
  if (!status_seen_) {
    if ((strncmp(line_, "HTTP/1.1 200", 12) && strncmp(line_, "HTTP/1.0 200", 12)) ||
        (line_[12] && line_[12] != ' ')) return false;
    status_seen_ = true;
    return true;
  }
  if (!line_[0]) {
    if (!length_seen_) return false;
    state_ = OtaDownloadState::Body;
    return true;
  }
  char* colon = strchr(line_, ':');
  if (!colon || colon == line_) return false;
  for (char* p = line_; p < colon; ++p) {
    if (*p >= 'A' && *p <= 'Z') *p += 'a' - 'A';
    if (!((*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') || *p == '-')) return false;
  }
  *colon = 0;
  char* value = colon + 1;
  while (*value == ' ' || *value == '\t') ++value;
  char* end = value + strlen(value);
  while (end > value && (end[-1] == ' ' || end[-1] == '\t')) *--end = 0;
  if (!strcmp(line_, "transfer-encoding")) return false;
  if (!strcmp(line_, "content-encoding") && strcmp(value, "identity")) return false;
  if (!strcmp(line_, "content-length")) {
    if (length_seen_ || !*value) return false;
    uint32_t size = 0;
    for (char* p = value; *p; ++p) {
      if (*p < '0' || *p > '9' || size > kOtaCapacity / 10) return false;
      size = size * 10 + *p - '0';
      if (size > kOtaCapacity) return false;
    }
    if (size < kOtaHeader + 32 + 20) return false;
    length_ = size; length_seen_ = true;
  }
  return true;
}
void OtaDownload::poll(uint32_t now, bool online) {
  if (!busy()) return;
  if (!online) { fail("Network disconnected"); return; }
  if (uint32_t(now - progress_) >= 15000 || uint32_t(now - started_) >= 300000) {
    fail("Transfer timeout"); return;
  }
  if (state_ == OtaDownloadState::Connecting) {
    const int result = io_.connected();
    if (result < 0) { fail("Connect failed"); return; }
    if (!result) return;
    state_ = OtaDownloadState::Sending; progress_ = now;
  }
  if (state_ == OtaDownloadState::Sending) {
    const int n = io_.send(reinterpret_cast<const uint8_t*>(request_) + sent_, request_size_ - sent_);
    if (n < 0) { fail("Request failed"); return; }
    if (!n) return;
    sent_ += n; progress_ = now;
    if (sent_ != request_size_) return;
    state_ = OtaDownloadState::Headers;
  }
  uint8_t block[1024];
  const int count = io_.receive(block, sizeof(block));
  if (count < 0) { fail("Connection ended before complete package"); return; }
  if (!count) return;
  progress_ = now;
  size_t pos = 0;
  while (pos < size_t(count) && state_ == OtaDownloadState::Headers) {
    const char c = block[pos++];
    if (++header_bytes_ > 4096 || line_size_ >= sizeof(line_) - 1 || !c) {
      fail("HTTP headers too long or invalid"); return;
    }
    if (c == '\n') {
      if (!line_size_ || line_[line_size_ - 1] != '\r') { fail("Invalid HTTP line"); return; }
      line_[--line_size_] = 0;
      if (!header_line()) { fail("Unsupported HTTP response"); return; }
      line_size_ = 0;
    } else {
      if (line_size_ && line_[line_size_ - 1] == '\r') { fail("Invalid HTTP line"); return; }
      line_[line_size_++] = c;
    }
  }
  if (state_ != OtaDownloadState::Body) return;
  if (size_t(count) - pos > length_ - received_) { fail("Excess response data"); return; }
  while (pos < size_t(count) && received_ < kOtaHeader) header_[received_++] = block[pos++];
  if (received_ == kOtaHeader && stage_.state() == OtaState::Failed) {
    result_ = stage_.begin(header_, length_);
    if (result_ != OtaResult::Ok) { fail("Package header/layout rejected"); return; }
  }
  if (pos < size_t(count)) {
    result_ = stage_.append(block + pos, count - pos);
    if (result_ != OtaResult::Ok) { fail("Staging write failed"); return; }
    received_ += count - pos;
  }
  if (received_ == length_) {
    io_.close();
    result_ = stage_.finish();
    if (result_ != OtaResult::Ok) { fail("Package verification failed"); return; }
    state_ = OtaDownloadState::Ready;
  }
}
}  // namespace mt7697
