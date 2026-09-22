// SPDX-License-Identifier: GPL-3.0-or-later
#include "rx_trace.h"
#include "tcp_diagnostics.h"
#include <atomic>
#include <stdio.h>
#include <stdarg.h>
extern "C" {
#include <hal_uart.h>
#include <lwip/netif.h>
#include <lwip/pbuf.h>
#include <lwip/tcpip.h>
err_t __real_tcpip_input(struct pbuf*, struct netif*);
}
namespace {
std::atomic<unsigned> station_frames{0}, ap_frames{0}, station_dhcp{0}, ap_dhcp{0};
}
// Read-only observation at the SDK Ethernet-to-lwIP boundary. Do not change
// filters, packet contents, ownership, or the original input return value.
extern "C" err_t __wrap_tcpip_input(struct pbuf* packet, struct netif* iface) {
  uint8_t header[82]; // Ethernet + maximum IPv4 header + UDP
  const unsigned length=pbuf_copy_partial(packet,header,sizeof(header),0);
  const bool reply=mt7697::is_dhcp_reply(header,length);
  if (iface->name[0]=='s' && iface->name[1]=='t') {
    ++station_frames;if (reply) ++station_dhcp;
  } else if (iface->name[0]=='a' && iface->name[1]=='p') {
    ++ap_frames;if (reply) ++ap_dhcp;
  }
  const auto result=__real_tcpip_input(packet,iface);
  mt7697::record_tcp_ingress(header,length,result);
  return result;
}
namespace mt7697 {
void network_trace_printf(const char* format, ...) {
  // Separate from SDK stdout so tracing does not enable vendor printf chatter.
  char line[256];
  va_list args;
  va_start(args,format);
  const int length=vsnprintf(line,sizeof(line),format,args);
  va_end(args);
  for (int i=0;i<length && i<int(sizeof(line)-1);++i)
    hal_uart_put_char(HAL_UART_0,line[i]);
}
void report_network_rx() {
  network_trace_printf("NET: RX frames STA=%u AP=%u DHCP replies STA=%u AP=%u\n",
      station_frames.load(),ap_frames.load(),station_dhcp.load(),ap_dhcp.load());
}
}
