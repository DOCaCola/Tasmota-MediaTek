// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <WiFiUdp.h>
enum class DNSReplyCode { NoError };
class DNSServer {
 public:
  void setErrorReplyCode(DNSReplyCode) {}
  bool start(uint16_t port, const char*, IPAddress address) { address_=address;return udp_.begin(port); }
  void stop() { udp_.stop(); }
  void processNextRequest() {
    int length=udp_.parsePacket();
    if (!length) return;
    uint8_t packet[512];
    const auto peer=udp_.remoteIP(); const auto port=udp_.remotePort();
    if (length<17 || length>480) {while(udp_.available()) udp_.read();return;}
    if (udp_.read(packet,length)!=length || (packet[2]&0xf8) || packet[4] || packet[5]!=1) return;
    size_t p=12;
    while (p<size_t(length) && packet[p]) {
      unsigned count=packet[p++];
      if (count>63 || p+count>=size_t(length)) return;
      p+=count;
    }
    if (p+5>size_t(length)) return;
    ++p; bool ipv4=packet[p]==0 && packet[p+1]==1 && packet[p+2]==0 && packet[p+3]==1;
    p+=4;
    packet[2]=0x81;packet[3]=0x80;packet[6]=0;packet[7]=ipv4?1:0;
    packet[8]=packet[9]=packet[10]=packet[11]=0;
    if (ipv4) {
      const uint8_t answer[]={0xc0,0x0c,0,1,0,1,0,0,0,0,0,4};
      memcpy(packet+p,answer,sizeof(answer));p+=sizeof(answer);
      for(unsigned i=0;i<4;++i)packet[p++]=address_[i];
    }
    if(udp_.beginPacket(peer,port)){udp_.write(packet,p);udp_.endPacket();}
  }
 private:
  WiFiUDP udp_;
  IPAddress address_;
};
