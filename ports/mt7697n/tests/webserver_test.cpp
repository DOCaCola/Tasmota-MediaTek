// SPDX-License-Identifier: GPL-3.0-or-later
#include "../platform/native_webserver.h"
#include <cassert>
#include <string>
#include <algorithm>
#include <cerrno>
extern "C" {
#include <lwip/sockets.h>
}
static std::string incoming, outgoing;
static size_t position, fragment=4096, send_limit=17;
static bool pending, connected, bind_fail, blocked_send;
static unsigned now, calls;
uint32_t millis() { return now; }
void delay(unsigned long n) { now+=n; }
extern "C" {
int lwip_ioctl(int,int,void*) {return 0;}
int lwip_socket(int,int,int) {return 1;}
int lwip_setsockopt(int,int,int,const void*,socklen_t) {return 0;}
int lwip_bind(int,const sockaddr*,socklen_t) {return bind_fail?-1:0;}
int lwip_listen(int,int) {return 0;}
int lwip_accept(int,sockaddr*,socklen_t*) {
  if (!pending) {errno=EAGAIN;return -1;}
  pending=false;connected=true;return 2;
}
int lwip_close(int fd) {if(fd==2)connected=false;return 0;}
uint16_t lwip_htons(uint16_t n) {return (n>>8)|(n<<8);}
int lwip_send(int,const void* data,size_t n,int) {
  if(blocked_send){errno=EAGAIN;return -1;}
  n=std::min(n,send_limit);outgoing.append(static_cast<const char*>(data),n);return n;
}
int lwip_recv(int,void* data,size_t n,int) {
  if(position==incoming.size()){errno=EAGAIN;return -1;}
  n=std::min({n,fragment,incoming.size()-position});
  memcpy(data,incoming.data()+position,n);position+=n;return n;
}
}
static TasmotaWebServer server(80);
static TasmotaWebServer* Webserver=&server;
static bool manager_mode=true;
static bool WifiIsInManagerMode() {return manager_mode;}
static bool ValidIpAddress(const char* text) {IPAddress ip;return ip.fromString(text);}
static String IPGetListeningAddressStr() {return String("192.168.4.1");}
static void WSSend(int code,const char* type,const char* body) {server.send(code,type,body);}
#define LOG_LEVEL_DEBUG 0
#define D_LOG_HTTP ""
#define D_REDIRECTED ""
#define CT_PLAIN "text/plain"
static void AddLog(int,const char*) {}
#include "captive_portal_function.inc"
static void missing() {
  if (!CaptivePortal()) server.send(404,"text/plain","Not found");
}
static void page() {
  ++calls;
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200,"text/html","");
  server.sendContent("hello"); server.sendContent("");
}
static void save() {
  ++calls;
  assert(server.method()==HTTP_POST);
  assert(server.arg("s1")=="My & WiFi");assert(server.arg("p1")=="correct+horse");
  server.send(200,"text/plain","saved");
}
static void auth() {
  ++calls;
  if(!server.authenticate("admin","secret")){server.requestAuthentication();return;}
  server.send(200,"text/plain","ok");
}
static void request(const std::string& input,size_t split=4096) {
  incoming=input;position=0;outgoing.clear();fragment=split;pending=true;
  for(unsigned i=0;i<20000 && (pending || connected);++i) {server.handleClient();++now;}
  assert(!connected);
}
int main() {
  server.on("/",HTTP_GET,page);server.on("/wi",HTTP_POST,save);server.on("/auth",HTTP_GET,auth);
  bind_fail=true;server.begin();assert(!server.listening());
  bind_fail=false;server.begin();assert(server.listening());
  request("GET / HTTP/1.1\r\nHost: lamp\r\n\r\n",1);
  assert(calls==1 && outgoing.find("Transfer-Encoding: chunked")!=std::string::npos);
  assert(outgoing.find("5\r\nhello\r\n0\r\n\r\n")!=std::string::npos);
  const std::string body="s1=My+%26+WiFi&p1=correct%2Bhorse";
  request("POST /wi HTTP/1.1\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: "+std::to_string(body.size())+"\r\n\r\n"+body,3);
  assert(calls==2 && outgoing.find("saved")!=std::string::npos);
  request("GET /auth HTTP/1.1\r\n\r\n");assert(outgoing.find("401")!=std::string::npos);
  request("GET /auth HTTP/1.1\r\nAuthorization: Basic YWRtaW46c2VjcmV0\r\n\r\n");assert(outgoing.find("200")!=std::string::npos);
  const unsigned before=calls;
  for(const char* invalid : {
    "GET /?a=1&a=2 HTTP/1.1\r\n\r\n",
    "GET /?p=%00 HTTP/1.1\r\n\r\n",
    "GET /?p=%Q0 HTTP/1.1\r\n\r\n",
    "GET / HTTP/1.1\r\nContent-Length: 0\r\nContent-Length: 0\r\n\r\n",
    "GET / HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\n",
    "POST /wi HTTP/1.1\r\nContent-Type: multipart/form-data\r\n\r\n",
    "GET / HTTP/1.1\r\nContent-Length: 999999\r\n\r\n",
    "GET / HTTP/1.1\r\n\r\njunk"
  }) { request(invalid); assert(calls==before); }
  request("GET / HTTP/1.1\r\nX: "+std::string(9000,'a'));assert(calls==before);
  request("GET / HTTP/1.1\r\n");assert(calls==before); // Timeout of incomplete request
  blocked_send=true;request("GET / HTTP/1.1\r\n\r\n");assert(calls==before+1);blocked_send=false;
  request("GET / HTTP/1.1\r\n\r\n");assert(calls==before+2); // Recovers after stalled client
  // Exercise the actual Tasmota captive handler through the native HTTP parser.
  server.onNotFound(missing);
  for (const char* probe : {
      "GET /hotspot-detect.html HTTP/1.1\r\nHost: captive.apple.com\r\n\r\n",
      "GET /connecttest.txt HTTP/1.1\r\nHost: www.msftconnecttest.com\r\n\r\n",
      "GET /generate_204 HTTP/1.1\r\nhost: connectivitycheck.gstatic.com\r\n\r\n"}) {
    request(probe,3);
    assert(outgoing.find("HTTP/1.1 302")!=std::string::npos);
    assert(outgoing.find("Location: http://192.168.4.1\r\n")!=std::string::npos);
  }
  request("GET /missing HTTP/1.1\r\nHost: 192.168.4.1\r\n\r\n");
  assert(outgoing.find("404")!=std::string::npos && outgoing.find("Location:")==std::string::npos);
  manager_mode=false;
  request("GET /hotspot-detect.html HTTP/1.1\r\nHost: captive.apple.com\r\n\r\n");
  assert(outgoing.find("404")!=std::string::npos && outgoing.find("Location:")==std::string::npos);
  server.close();assert(!server.listening());
  puts("Native webserver: fragmented forms, chunked output, authentication, framing rejection, timeouts and recovery passed.");
}
