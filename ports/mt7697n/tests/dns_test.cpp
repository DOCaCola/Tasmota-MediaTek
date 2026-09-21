// SPDX-License-Identifier: GPL-3.0-or-later
#include "../arduino/DNSServer.h"
#include <cassert>
#include <cstdio>
static DNSServer server;
static void query(std::vector<uint8_t> bytes) {
  dns_input=bytes;dns_position=0;dns_output.clear();server.processNextRequest();
}
int main() {
  assert(server.start(53,"*",IPAddress(192,168,4,1)) && udp_bound);
  const std::vector<uint8_t> a={0x12,0x34,1,0,0,1,0,0,0,0,0,0,4,'l','a','m','p',0,0,1,0,1};
  query(a);assert(dns_output.size()==a.size()+16);
  assert(dns_output[0]==0x12 && dns_output[1]==0x34 && dns_output[7]==1);
  assert((std::vector<uint8_t>(dns_output.end()-4,dns_output.end())==std::vector<uint8_t>{192,168,4,1}));
  auto ipv6=a;ipv6[19]=28;query(ipv6);assert(dns_output.size()==a.size() && dns_output[7]==0);
  auto compressed=a;compressed[12]=0xc0;query(compressed);assert(dns_output.empty());
  auto multiple=a;multiple[5]=2;query(multiple);assert(dns_output.empty());
  auto response=a;response[2]=0x81;query(response);assert(dns_output.empty());
  for(size_t n=0;n<a.size();++n){query(std::vector<uint8_t>(a.begin(),a.begin()+n));assert(dns_output.empty());}
  query(std::vector<uint8_t>(600,0));assert(dns_output.empty() && dns_position==600);
  server.stop();assert(!udp_bound);
  puts("Captive DNS: A/AAAA responses, transaction identity, malformed/truncated/oversized requests passed.");
}
