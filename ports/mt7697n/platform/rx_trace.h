// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <stddef.h>
#include <stdint.h>
namespace mt7697 {
// Inspect only headers. The caller never consumes or modifies the packet.
inline bool is_dhcp_reply(const uint8_t* h, size_t n) {
  if (n < 42 || h[12] != 8 || h[13] != 0 || h[14] >> 4 != 4) return false;
  const unsigned ihl = (h[14] & 15) * 4;
  if (ihl < 20 || n < 14 + ihl + 8 || h[23] != 17) return false;
  // Non-initial fragments do not contain the UDP header.
  if ((h[20] & 31) || h[21]) return false;
  const auto* udp = h + 14 + ihl;
  return udp[0] == 0 && udp[1] == 67 && udp[2] == 0 && udp[3] == 68;
}
void report_network_rx();
void network_trace_printf(const char* format, ...) __attribute__((format(printf,1,2)));
}
