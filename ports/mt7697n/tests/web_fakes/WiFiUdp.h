#pragma once
#include <IPAddress.h>
#include <vector>
#include <cstring>
inline std::vector<uint8_t> dns_input,dns_output;
inline size_t dns_position;
inline bool udp_bound;
class WiFiUDP {
 public:
  int begin(uint16_t) {udp_bound=true;return 1;}
  void stop() {udp_bound=false;}
  int parsePacket() {return dns_input.size();}
  IPAddress remoteIP() {return IPAddress(192,168,4,2);}
  uint16_t remotePort() {return 1500;}
  int available() {return dns_input.size()-dns_position;}
  int read() {return available()?dns_input[dns_position++]:-1;}
  int read(uint8_t* out,size_t n) {n=std::min(n,dns_input.size()-dns_position);memcpy(out,dns_input.data()+dns_position,n);dns_position+=n;return n;}
  int beginPacket(IPAddress,uint16_t) {dns_output.clear();return 1;}
  size_t write(const uint8_t* bytes,size_t n) {dns_output.insert(dns_output.end(),bytes,bytes+n);return n;}
  int endPacket() {return 1;}
};
