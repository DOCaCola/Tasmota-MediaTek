// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <Arduino.h>
#include <IPAddress.h>
enum HTTPMethod { HTTP_ANY, HTTP_GET, HTTP_HEAD, HTTP_POST, HTTP_OPTIONS };
constexpr size_t CONTENT_LENGTH_UNKNOWN = size_t(-1);
struct NativeWebStats {
  uint32_t accepted=0,completed=0,send_waits=0,send_errors=0,close_errors=0;
  uint32_t timeouts=0,queue_failures=0,peak_queued=0;
  int last_send_error=0,last_close_error=0,last_wait_error=0;
};
const NativeWebStats& NativeWebStatistics();

class NativeWebClient {
 public:
  size_t write(const char* data, size_t size);
  size_t write(const uint8_t* data, size_t size) { return write(reinterpret_cast<const char*>(data), size); }
  void stop();
  void flush() {} // Queued output drains through handleClient(), without blocking.
  IPAddress remoteIP() const { return peer_; }
 private:
  friend class TasmotaWebServer;
  struct Block { Block* next; size_t size; char data[1024]; };
  void clear();
  void abortResponse();
  void pump();
  void release();
  Block* head_=nullptr;
  Block* tail_=nullptr;
  size_t offset_=0,queued_=0;
  uint32_t progress_=0;
  bool finishing_=false,closing_=false,failed_=false;
  bool keep_alive_=false,reusable_=false;
  size_t failure_offset_=0;
  int socket_ = -1;
  IPAddress peer_;
};

class TasmotaWebServer {
 public:
  explicit TasmotaWebServer(int port) : port_(port) {}
  void on(const char* uri, HTTPMethod method, void (*handler)());
  void onNotFound(void (*handler)()) { missing_ = handler; }
  void collectHeaders(const char**, size_t) {} // All headers are collected.
  void begin();
  void close();
  bool listening() const { return listener_ready_; }
  void handleClient();
  NativeWebClient& client() { return client_; }
  String arg(const String& name) const;
  String arg(unsigned index) const;
  String argName(unsigned index) const;
  unsigned args() const { return arg_count_; }
  bool hasArg(const String& name) const;
  String header(const String& name) const;
  String hostHeader() const { return header("Host"); }
  String uri() const { return uri_; }
  HTTPMethod method() const { return method_; }
  bool authenticate(const char* user, const char* password) const;
  void requestAuthentication();
  void sendHeader(const String& name, const String& value, bool first = false);
  void setContentLength(size_t length) { content_length_ = length; }
  void send(int code, const char* type, const String& body);
  void sendContent(const String& body);
  void sendContent(const char* data, size_t size);
  bool isChunked() const { return chunked_; }
 private:
  struct Pair { String name, value; };
  struct Route { String uri; HTTPMethod method; void (*handler)(); };
  bool parse();
  bool parseArgs(const char* text);
  void reset();
  void openListener();
  void releaseListener();
  bool wanted_=false,listener_ready_=false;
  uint32_t last_open_=0;
  int port_, listener_ = -1;
  NativeWebClient client_;
  NativeWebClient idle_[3];
  unsigned idle_cursor_=0;
  Route routes_[32];
  unsigned route_count_ = 0;
  void (*missing_)() = nullptr;
  Pair arguments_[64], headers_[32];
  unsigned arg_count_ = 0, header_count_ = 0;
  char request_[8193];
  size_t received_ = 0, expected_ = 0, content_length_ = 0;
  uint32_t started_ = 0;
  String uri_, response_headers_;
  HTTPMethod method_ = HTTP_ANY;
  bool chunked_ = false;
};
