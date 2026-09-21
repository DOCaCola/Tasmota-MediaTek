// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <Arduino.h>
#include <IPAddress.h>
enum HTTPMethod { HTTP_ANY, HTTP_GET, HTTP_HEAD, HTTP_POST, HTTP_OPTIONS };
constexpr size_t CONTENT_LENGTH_UNKNOWN = size_t(-1);

class NativeWebClient {
 public:
  size_t write(const char* data, size_t size);
  size_t write(const uint8_t* data, size_t size) { return write(reinterpret_cast<const char*>(data), size); }
  void stop();
  void flush() {} // Writes are completed by write(); no buffered output.
  IPAddress remoteIP() const { return peer_; }
 private:
  friend class TasmotaWebServer;
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
  bool listening() const { return listener_ >= 0; }
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
  int port_, listener_ = -1;
  NativeWebClient client_;
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
