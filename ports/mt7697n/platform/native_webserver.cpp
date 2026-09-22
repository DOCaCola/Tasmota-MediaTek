// SPDX-License-Identifier: GPL-3.0-or-later
#include "native_webserver.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
extern "C" {
#include <lwip/sockets.h>
}

namespace {
NativeWebStats statistics;
bool temporary(int error) {
  return error==EAGAIN || error==EWOULDBLOCK || error==ENOMEM ||
         error==ENOBUFS || error==EINTR;
}
const char unavailable[]="HTTP/1.1 503 Service Unavailable\r\nConnection: close\r\n"
                        "Content-Length: 0\r\n\r\n";
bool nonblock(int fd) {
  unsigned long enabled = 1;
  return lwip_ioctl(fd, FIONBIO, &enabled) == 0;
}
int hex(char c) {
  if (c >= '0' && c <= '9') return c-'0';
  if (c >= 'a' && c <= 'f') return c-'a'+10;
  if (c >= 'A' && c <= 'F') return c-'A'+10;
  return -1;
}
bool decode(const char* p, size_t length, String& out) {
  out = "";
  for (size_t i=0;i<length;++i) {
    char c=p[i];
    if (c=='+') c=' ';
    else if (c=='%') {
      if (i+2>=length || hex(p[i+1])<0 || hex(p[i+2])<0) return false;
      c=hex(p[i+1])*16+hex(p[i+2]); i+=2;
    }
    if (!c || c=='\r' || c=='\n') return false;
    if (!out.concat(c)) return false;
  }
  return true;
}
}
const NativeWebStats& NativeWebStatistics() { return statistics; }

size_t NativeWebClient::write(const char* data, size_t size) {
  if (socket_<0 || failed_ || finishing_) return 0;
  // Stage the complete response before transmission: allocation/size failures
  // produce a complete 503, never a header followed by truncated HTML.
  if (size>32768-queued_) { clear();failed_=true;keep_alive_=false;++statistics.queue_failures;return 0; }
  size_t copied=0;
  while (copied<size) {
    if (!tail_ || tail_->size==sizeof(tail_->data)) {
      auto* block=static_cast<Block*>(malloc(sizeof(Block)));
      if (!block) { clear();failed_=true;keep_alive_=false;++statistics.queue_failures;return 0; }
      block->next=nullptr;block->size=0;
      if (tail_) tail_->next=block;else head_=block;
      tail_=block;
    }
    size_t n=sizeof(tail_->data)-tail_->size;
    if (n>size-copied) n=size-copied;
    memcpy(tail_->data+tail_->size,data+copied,n);
    tail_->size+=n;queued_+=n;copied+=n;
  }
  if (queued_>statistics.peak_queued) statistics.peak_queued=queued_;
  return copied;
}
void NativeWebClient::stop() {
  // Tasmota calls stop() at the end of a generated response. Preserve queued
  // bytes until accepted by TCP; closing a response is not an abort.
  if (socket_>=0 && !finishing_) { finishing_=true;progress_=millis(); }
}
void NativeWebClient::clear() {
  while (head_) { Block* next=head_->next;free(head_);head_=next; }
  tail_=nullptr;offset_=queued_=0;
}
void NativeWebClient::abortResponse() {
  clear();finishing_=closing_=true;failed_=keep_alive_=reusable_=false;
}
void NativeWebClient::release() {
  if (socket_>=0 && lwip_close(socket_)<0 && errno!=EBADF) {
    ++statistics.close_errors;statistics.last_close_error=errno;
    // SDK leaves a failed-close descriptor allocated. Retain ownership and
    // retry on a later main-loop pass rather than leaking it.
    return;
  }
  socket_=-1;finishing_=closing_=failed_=keep_alive_=reusable_=false;failure_offset_=0;
}
void NativeWebClient::pump() {
  if (socket_<0 || !finishing_) return;
  if (!closing_ && uint32_t(millis()-progress_)>=5000) {
    ++statistics.timeouts;abortResponse();
  }
  unsigned budget=4096;
  while (!closing_ && budget && (head_ || failed_)) {
    const char* data=failed_?unavailable+failure_offset_:head_->data+offset_;
    size_t size=failed_?sizeof(unavailable)-1-failure_offset_:head_->size-offset_;
    if (size>budget) size=budget;
    int n=lwip_send(socket_,data,size,0);
    if (n>0) {
      progress_=millis();budget-=n;
      if (failed_) {
        failure_offset_+=n;
        if (failure_offset_==sizeof(unavailable)-1) { failed_=false;break; }
      } else {
        offset_+=n;queued_-=n;
        if (offset_==head_->size) {
          Block* next=head_->next;free(head_);head_=next;offset_=0;
          if (!head_) tail_=nullptr;
        }
      }
    } else {
      const int error=n<0?errno:0;
      if (!n || temporary(error)) {
        ++statistics.send_waits;statistics.last_wait_error=error;break;
      }
      ++statistics.send_errors;statistics.last_send_error=error;
      abortResponse();
    }
  }
  if (!closing_ && !head_ && !failed_) {
    ++statistics.completed;
    if (keep_alive_) { finishing_=false;reusable_=true;return; }
    closing_=true;
  }
  if (closing_) release();
}
void TasmotaWebServer::on(const char* uri, HTTPMethod method, void (*handler)()) {
  if (route_count_ == 32) abort(); // Programming error, never silently omit routes.
  routes_[route_count_++]={String(uri),method,handler};
}
void TasmotaWebServer::begin() {
  close();
  wanted_=true;
  openListener();
}
void TasmotaWebServer::openListener() {
  if (listener_>=0 || client_.socket_>=0) return;
  last_open_=millis();
  listener_=lwip_socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
  if (listener_<0) return;
  sockaddr_in address={};
  address.sin_family=AF_INET; address.sin_port=lwip_htons(port_);
  int reuse=1; lwip_setsockopt(listener_,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse));
  if (!nonblock(listener_) || lwip_bind(listener_,reinterpret_cast<sockaddr*>(&address),sizeof(address))<0 ||
      lwip_listen(listener_,1)<0) { releaseListener();return; }
  listener_ready_=true;
}
void TasmotaWebServer::close() {
  wanted_=listener_ready_=false;
  client_.abortResponse();client_.release();
  for (auto& idle:idle_) { idle.abortResponse();idle.release(); }
  releaseListener();
}
void TasmotaWebServer::releaseListener() {
  if (listener_>=0) {
    if (lwip_close(listener_)==0 || errno==EBADF) listener_=-1;
    else { ++statistics.close_errors;statistics.last_close_error=errno; }
  }
}
void TasmotaWebServer::reset() {
  for (unsigned i=0;i<arg_count_;++i) arguments_[i]={};
  for (unsigned i=0;i<header_count_;++i) headers_[i]={};
  arg_count_=header_count_=0;
  received_=expected_=content_length_=0;
  request_[0]=0; uri_="";response_headers_="";chunked_=false;method_=HTTP_ANY;
}
bool TasmotaWebServer::parseArgs(const char* p) {
  while (*p) {
    const char* end=strchr(p,'&'); if (!end) end=p+strlen(p);
    const char* eq=static_cast<const char*>(memchr(p,'=',end-p));
    if (arg_count_==64) return false;
    Pair& a=arguments_[arg_count_];
    a={};
    if (!decode(p,(eq ? eq:end)-p,a.name) || !a.name.length()) return false;
    if (hasArg(a.name)) return false;
    if (eq && !decode(eq+1,end-eq-1,a.value)) return false;
    ++arg_count_;
    p=*end ? end+1:end;
  }
  return true;
}
bool TasmotaWebServer::parse() {
  char* end=strstr(request_,"\r\n\r\n"); if (!end) return false;
  char* line=strstr(request_,"\r\n"); *line=0;
  char* path=strchr(request_,' '); if (!path) return false; *path++=0;
  char* version=strchr(path,' '); if (!version) return false; *version++=0;
  if (strcmp(version,"HTTP/1.1") && strcmp(version,"HTTP/1.0")) return false;
  const bool http11=!strcmp(version,"HTTP/1.1");
  if (!strcmp(request_,"GET")) method_=HTTP_GET;
  else if (!strcmp(request_,"POST")) method_=HTTP_POST;
  else if (!strcmp(request_,"HEAD")) method_=HTTP_HEAD;
  else if (!strcmp(request_,"OPTIONS")) method_=HTTP_OPTIONS;
  else return false;
  if (*path!='/') return false;
  char* query=strchr(path,'?'); if (query) *query++=0;
  uri_=path;
  for (char* p=line+2;p<end;) {
    char* next=strstr(p,"\r\n"); if (!next) return false; *next=0;
    char* colon=strchr(p,':'); if (!colon || colon==p || header_count_==32) return false;
    *colon++=0;
    for (char* q=p;*q;++q) if (!( (*q>='A' && *q<='Z') || (*q>='a' && *q<='z') || *q=='-' || (*q>='0' && *q<='9'))) return false;
    while (*colon==' ' || *colon=='\t') ++colon;
    headers_[header_count_++]={String(p),String(colon)};
    p=next+2;
  }
  String connection=header("Connection");
  connection.toLowerCase();
  client_.keep_alive_=http11 && connection.indexOf("close")<0;
  if (query && !parseArgs(query)) return false;
  if (method_==HTTP_POST) {
    String type=header("Content-Type");
    if (!type.startsWith("application/x-www-form-urlencoded")) return false;
    if (!parseArgs(end+4)) return false;
  }
  return true;
}
void TasmotaWebServer::handleClient() {
  // Keep only idle sockets here: request parsing and response generation retain
  // one owner, while idle browsers cannot monopolize the server.
  for (auto& idle:idle_) {
    if (idle.closing_) idle.release();
    if (idle.socket_>=0 && uint32_t(millis()-idle.progress_)>=15000) {
      idle.abortResponse();idle.release();
    }
  }
  if (client_.reusable_) {
    bool parked=false;
    for (auto& idle:idle_) {
      if (idle.socket_<0) {
        idle.socket_=client_.socket_;idle.peer_=client_.peer_;idle.progress_=millis();
        client_.socket_=-1;client_.reusable_=client_.keep_alive_=false;
        parked=true;break;
      }
    }
    if (!parked) { client_.abortResponse();client_.release(); }
  }
  if (client_.finishing_) { client_.pump();return; }
  if (!listener_ready_) {
    releaseListener();
    if (wanted_ && uint32_t(millis()-last_open_)>=1000) openListener();
    return;
  }
  if (client_.socket_<0) {
    // Round-robin ready persistent connections. Peek never consumes bytes;
    // normal framing/parser validation still owns the entire request.
    for (unsigned i=0;i<3;++i) {
      auto& idle=idle_[(idle_cursor_+i)%3];
      if (idle.socket_<0 || idle.closing_) continue;
      char byte;
      int n=lwip_recv(idle.socket_,&byte,1,MSG_PEEK|MSG_DONTWAIT);
      if (n==0 || (n<0 && errno!=EAGAIN && errno!=EWOULDBLOCK)) {
        idle.abortResponse();idle.release();continue;
      }
      if (n>0) {
        reset();client_.socket_=idle.socket_;client_.peer_=idle.peer_;
        idle.socket_=-1;started_=millis();idle_cursor_=(idle_cursor_+i+1)%3;
        break;
      }
    }
  }
  if (client_.socket_<0) {
    sockaddr_in peer={};socklen_t length=sizeof(peer);
    int fd=lwip_accept(listener_,reinterpret_cast<sockaddr*>(&peer),&length);
    if (fd<0) return;
    reset();client_.socket_=fd;client_.peer_=IPAddress(peer.sin_addr.s_addr);started_=millis();
    ++statistics.accepted;
    if (!nonblock(fd)) {client_.abortResponse();return;}
  }
  if (uint32_t(millis()-started_)>5000 || received_==sizeof(request_)-1) {client_.stop();return;}
  int n=lwip_recv(client_.socket_,request_+received_,sizeof(request_)-1-received_,MSG_DONTWAIT);
  if (n<=0) {
    if (!n || (errno!=EAGAIN && errno!=EWOULDBLOCK)) client_.stop();
    return;
  }
  if (memchr(request_+received_,0,n)) {client_.stop();return;}
  received_+=n;request_[received_]=0;
  if (!expected_) {
    char* end=strstr(request_,"\r\n\r\n");
    if (!end) return;
    size_t body=0; bool length_seen=false;
    // Determine bounded framing before modifying the request.
    for (char* p=strstr(request_,"\r\n")+2;p<end;) {
      char* next=strstr(p,"\r\n"); if (!next) {client_.stop();return;}
      if (!strncasecmp(p,"Transfer-Encoding:",18)) {client_.stop();return;}
      if (!strncasecmp(p,"Content-Length:",15)) {
        if (length_seen) {client_.stop();return;} length_seen=true;
        char* q=p+15;while (q<next && *q==' ')++q;
        if (q==next) {client_.stop();return;}
        for (;q<next;++q) {
          if (*q<'0' || *q>'9' || body>8192/10) {client_.stop();return;}
          body=body*10+*q-'0';
        }
      }
      p=next+2;
    }
    expected_=end+4-request_+body;
    if (expected_>=sizeof(request_)) {client_.stop();return;}
  }
  if (received_<expected_) return;
  if (received_!=expected_ || !parse()) {
    client_.keep_alive_=false;
    send(400,"text/plain","Invalid or unsupported request");client_.stop();return;
  }
  void (*handler)()=missing_;
  for (unsigned i=0;i<route_count_;++i)
    if (routes_[i].uri==uri_ && (routes_[i].method==HTTP_ANY || routes_[i].method==method_)) {handler=routes_[i].handler;break;}
  if (handler) handler(); else send(404,"text/plain","Not found");
  client_.stop(); // Finish this response; reusable sockets return to the idle set.
}
String TasmotaWebServer::arg(const String& name) const {
  for (unsigned i=0;i<arg_count_;++i) if (arguments_[i].name==name) return arguments_[i].value;
  return String();
}
String TasmotaWebServer::arg(unsigned i) const {return i<arg_count_?arguments_[i].value:String();}
String TasmotaWebServer::argName(unsigned i) const {return i<arg_count_?arguments_[i].name:String();}
bool TasmotaWebServer::hasArg(const String& name) const {
  for (unsigned i=0;i<arg_count_;++i) if (arguments_[i].name==name) return true;
  return false;
}
String TasmotaWebServer::header(const String& name) const {
  for (unsigned i=0;i<header_count_;++i) if (headers_[i].name.equalsIgnoreCase(name)) return headers_[i].value;
  return String();
}
bool TasmotaWebServer::authenticate(const char* user, const char* password) const {
  String plain=String(user)+":"+password, encoded="Basic ";
  const char* table="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  for (unsigned i=0;i<plain.length();i+=3) {
    unsigned remaining=plain.length()-i;
    uint32_t v=uint8_t(plain[i])<<16;
    if (remaining>1) v|=uint8_t(plain[i+1])<<8;
    if (remaining>2) v|=uint8_t(plain[i+2]);
    encoded+=table[(v>>18)&63];encoded+=table[(v>>12)&63];
    encoded+=remaining>1?table[(v>>6)&63]:'=';encoded+=remaining>2?table[v&63]:'=';
  }
  return header("Authorization")==encoded;
}
void TasmotaWebServer::requestAuthentication() {
  sendHeader("WWW-Authenticate","Basic realm=\"Tasmota\"");
  send(401,"text/plain","Authentication required");
}
void TasmotaWebServer::sendHeader(const String& name,const String& value,bool first) {
  String h=name+": "+value+"\r\n";
  if (first) response_headers_=h+response_headers_;else response_headers_+=h;
}
void TasmotaWebServer::send(int code,const char* type,const String& body) {
  chunked_=content_length_==CONTENT_LENGTH_UNKNOWN;
  String h="HTTP/1.1 "+String(code)+" Response\r\nContent-Type: "+type;
  h+=client_.keep_alive_ ? "\r\nConnection: keep-alive\r\nKeep-Alive: timeout=15\r\n" :
                         "\r\nConnection: close\r\n";
  if (chunked_) h+="Transfer-Encoding: chunked\r\n";
  else h+="Content-Length: "+String(static_cast<unsigned long>(content_length_?content_length_:body.length()))+"\r\n";
  h+=response_headers_+"\r\n";
  client_.write(h.c_str(),h.length());
  if (body.length() && method_!=HTTP_HEAD) sendContent(body);
}
void TasmotaWebServer::sendContent(const String& body) {
  sendContent(body.c_str(),body.length());
}
void TasmotaWebServer::sendContent(const char* data,size_t length) {
  if (method_==HTTP_HEAD) return;
  if (chunked_) {
    char size[16];snprintf(size,sizeof(size),"%x\r\n",unsigned(length));
    client_.write(size,strlen(size));client_.write(data,length);client_.write("\r\n",2);
  } else client_.write(data,length);
}
