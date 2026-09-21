// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <platform/ota_http.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <vector>
#include <string>
#include <fstream>
#include <algorithm>
using namespace mt7697;

struct MemoryFlash : OtaFlash {
  std::vector<uint8_t> bytes = std::vector<uint8_t>(kOtaLength, 0xff);
  unsigned writes = 0, activations = 0;
  bool activation_ok = false;
  bool layout_matches() override { return true; }
  bool read(uint32_t p, void* out, size_t n) override { memcpy(out, bytes.data()+p, n); return true; }
  bool erase_sector(uint32_t p) override { memset(bytes.data()+p, 0xff, 4096); return true; }
  bool write(uint32_t p, const void* in, size_t n) override {
    ++writes; memcpy(bytes.data()+p, in, n); return true;
  }
  bool activate() override {
    ++activations;
    if (activation_ok) memcpy(bytes.data()+kOtaLength-512, "MMM", 4);
    return activation_ok;
  }
};
struct Transport : OtaTransport {
  std::vector<uint8_t> response;
  std::string request;
  size_t pos = 0, fragment = 31;
  bool closed = true, stall = false, connect_error = false;
  int connection = 1;
  bool connect(const OtaUrl&) override { closed = false; return !connect_error; }
  int connected() override { return connection; }
  int send(const uint8_t* p, size_t n) override {
    if (stall) return 0;
    n = std::min(n, fragment); request.append(reinterpret_cast<const char*>(p), n); return n;
  }
  int receive(uint8_t* out, size_t n) override {
    if (stall) return 0;
    if (pos == response.size()) return -1;
    n = std::min({n, fragment, response.size()-pos});
    memcpy(out,response.data()+pos,n); pos += n; return n;
  }
  void close() override { closed = true; }
};
inline std::vector<uint8_t> response(const std::vector<uint8_t>& body, const std::string& headers = "") {
  std::string h = "HTTP/1.1 200 OK\r\nContent-Length: " + std::to_string(body.size()) + "\r\n" + headers + "\r\n";
  std::vector<uint8_t> r(h.begin(), h.end()); r.insert(r.end(),body.begin(),body.end()); return r;
}
inline void run(OtaDownload& d, uint32_t start = 0) {
  for (uint32_t i = 0; d.busy() && i < 20000; ++i) d.poll(start+i, true);
  assert(!d.busy());
}
