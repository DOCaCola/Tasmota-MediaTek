// SPDX-License-Identifier: GPL-3.0-or-later
#include "../platform/rx_trace.h"
#include <cassert>
#include <cstdio>
int main() {
  uint8_t frame[82]{};
  frame[12]=8;frame[14]=0x45;frame[23]=17;
  frame[35]=67;frame[37]=68;
  assert(mt7697::is_dhcp_reply(frame,42));
  for (unsigned n=0;n<42;++n) assert(!mt7697::is_dhcp_reply(frame,n));
  frame[21]=1;assert(!mt7697::is_dhcp_reply(frame,42));frame[21]=0;
  frame[20]=0x20;assert(mt7697::is_dhcp_reply(frame,42));frame[20]=0;
  frame[23]=6;assert(!mt7697::is_dhcp_reply(frame,42));frame[23]=17;
  frame[35]=68;frame[37]=67;assert(!mt7697::is_dhcp_reply(frame,42));
  frame[14]=0x4f;frame[75]=67;frame[77]=68;
  assert(mt7697::is_dhcp_reply(frame,82));
  assert(!mt7697::is_dhcp_reply(frame,81));
  frame[14]=0x44;assert(!mt7697::is_dhcp_reply(frame,82));
  frame[14]=0x65;assert(!mt7697::is_dhcp_reply(frame,82));
  puts("RX trace: bounded DHCP header classification passed");
}
