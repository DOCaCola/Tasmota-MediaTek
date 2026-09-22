// SPDX-License-Identifier: GPL-3.0-or-later
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include "../platform/sdk_network.h"
#include <LWiFi.h>
static netif iface{};
static bool initialized=false, tcpip=false, missing=false;
static int fail_call=0, calls=0, dhcp_calls=0, dhcp_error=0, mailbox_error=0;
static uint8_t link_status=0;
static uint8_t radio_mode=WIFI_MODE_STA_ONLY;
static bool mode_query_error=false;
static int driver_ip_mode=0,ready_calls=0,ready_result=0;
static std::string sequence;
static int operation(char code) { sequence+=code; return ++calls==fail_call ? -1 : 0; }
namespace mt7697 { bool apply_setup_ap() {
  assert(radio_mode==WIFI_MODE_REPEATER);return operation('P')>=0;
}
void report_network_rx() {}
void network_trace_printf(const char*, ...) {}
}
extern "C" {
const ip_addr_t zero_ip{};
void init_global_connsys() { initialized=true; }
bool wifi_ready() { return initialized; }
int wifi_config_set_ip_mode(uint8_t mode) {
  assert(mode==STA_IP_MODE_DHCP);
  int result=operation('I');if(result==0)driver_ip_mode=mode;return result;
}
int wifi_config_get_bssid(uint8_t* mac) {memset(mac,0,6);return 0;}
int wifi_config_get_channel(uint8_t,uint8_t* channel) {*channel=1;return 0;}
int wifi_config_get_rx_filter(uint32_t* filter) {*filter=0;return 0;}
int wifi_connection_inform_ip_ready() {
  assert(!tcpip && driver_ip_mode==STA_IP_MODE_DHCP);
  assert(iface.lease && iface.ip_addr.addr);
  ++ready_calls;return ready_result;
}
int wifi_config_get_opmode(uint8_t* mode) {*mode=radio_mode;return mode_query_error?-1:0;}
int wifi_config_set_opmode(uint8_t mode) {
  int result=operation('M');if(result==0)radio_mode=mode;return result;
}
int wifi_config_set_radio(uint8_t on) {
  (void)on;assert(false && "Station lifecycle must not toggle shared radio");return -1;
}
int wifi_connection_disconnect_ap() {int result=operation('D');if (result==0)link_status=0;return result;}
int wifi_config_set_ssid(uint8_t port,uint8_t*,uint8_t) { assert(port==0);return operation('S'); }
int wifi_config_set_security_mode(uint8_t port,int auth,int encryption) {
  assert(port==0);assert((auth==WIFI_AUTH_MODE_WPA2_PSK && encryption==WIFI_ENCRYPT_TYPE_AES_ENABLED)||
      (auth==WIFI_AUTH_MODE_OPEN && encryption==WIFI_ENCRYPT_TYPE_WEP_DISABLED));return operation('A');
}
int wifi_config_set_wpa_psk_key(uint8_t,uint8_t*,uint8_t) {return operation('K');}
int wifi_config_reload_setting() {assert(driver_ip_mode==STA_IP_MODE_DHCP);return operation('R');}
int wifi_connection_get_link_status(uint8_t* value) {*value=link_status;return 0;}
netif* netif_find_by_type(int) {assert(tcpip);return missing?nullptr:&iface;}
void netif_set_link_down(netif* n) {assert(tcpip);n->link=false;}
void netif_set_link_up(netif* n) {assert(tcpip);n->link=true;}
void netif_set_up(netif* n) {assert(tcpip);n->up=true;}
void netif_set_default(netif*) {assert(tcpip);}
void netif_set_addr(netif* n,const ip_addr_t* ip,const ip_addr_t*,const ip_addr_t*) {assert(tcpip);n->ip_addr=*ip;}
int netif_is_link_up(netif* n) {assert(tcpip);return n->link;}
int dhcp_start(netif*) {assert(tcpip);++dhcp_calls;return dhcp_error;}
void dhcp_stop(netif* n) {assert(tcpip);n->lease=false;}
int dhcp_supplied_address(netif* n) {assert(tcpip);return n->lease;}
int sys_sem_new(sys_sem_t* s,int) {*s=0;return 0;}
void sys_sem_free(sys_sem_t*) {}
void sys_sem_signal(sys_sem_t* s) {*s=1;}
void sys_arch_sem_wait(sys_sem_t* s,int) {assert(*s==1);}
int tcpip_callback(void(*f)(void*),void* p) {
  if(mailbox_error)return -1;
  tcpip=true;f(p);tcpip=false;return 0;
}
}
int main() {
  using namespace mt7697;
  assert(station_stop()); // no radio call before initialization
  assert(sequence.empty());
  assert(station_start("router","password"));assert(sequence=="ISAKR");
  assert(!station_online() && dhcp_calls==0);
  assert(ready_calls==0);
  link_status=1;assert(!station_online() && dhcp_calls==1);
  assert(!station_online() && dhcp_calls==1); // no DHCP restart per poll
  assert(ready_calls==0); // association/discovery is not IP-ready
  iface.ip_addr.addr=123;iface.lease=true;assert(station_online());
  assert(ready_calls==1);assert(station_online() && ready_calls==1);
  // New credentials never inherit a previous lease, even for the same SSID.
  assert(station_start("router","wrong-pass"));assert(!station_online());
  assert(iface.ip_addr.addr==0 && !iface.lease);
  link_status=1;assert(!station_online());iface.ip_addr.addr=456;iface.lease=true;
  assert(station_online());link_status=0;assert(!station_online());
  assert(iface.ip_addr.addr==0 && !iface.link);
  sequence.clear();assert(station_start("open",""));assert(sequence=="IDSAR");
  assert(station_stop());assert(!station_online());
  // Every configuration error blocks success and can be cleaned up.
  for (int i=1;i<=6;++i) {
    calls=0;fail_call=i;assert(!station_start("router","password"));
    assert(!station_online());fail_call=0;assert(station_stop());
  }
  missing=true;assert(!station_start("router","password"));missing=false;
  mailbox_error=1;assert(!station_start("router","password"));mailbox_error=0;
  assert(station_start("router","password"));link_status=1;dhcp_error=-1;
  assert(!station_online() && !iface.link);dhcp_error=0;assert(!station_online() && iface.link);
  radio_mode=WIFI_MODE_AP_ONLY;sequence.clear();
  assert(station_stop() && sequence.empty()); // no unsupported disconnect in AP-only
  assert(station_start("trial","password"));
  assert(radio_mode==WIFI_MODE_REPEATER && sequence=="IMPDSAKR");
  radio_mode=WIFI_MODE_AP_ONLY;calls=0;fail_call=2;
  assert(!station_start("trial","password") && radio_mode==WIFI_MODE_AP_ONLY);
  fail_call=0;mode_query_error=true;
  assert(!station_start("trial","password") && !station_stop());
  mode_query_error=false;
  // SDK link-up is not proof that this owner has started a DHCP client.
  assert(station_start("router","password"));
  iface.link=true;link_status=1;
  const int before=dhcp_calls;
  assert(!station_online() && dhcp_calls==before+1);
  assert(!station_online() && dhcp_calls==before+1);
  link_status=0;assert(!station_online());
  iface.link=true;link_status=1;
  assert(!station_online() && dhcp_calls==before+2);
  // Failed AP restoration must never reload a default-named hotspot.
  radio_mode=WIFI_MODE_AP_ONLY;calls=0;fail_call=3;sequence.clear();
  assert(!station_start("trial","password") && sequence=="IMP");
  fail_call=0;
  assert(station_start("router","password"));link_status=1;
  assert(!station_online());iface.ip_addr.addr=789;iface.lease=true;
  const int notified=ready_calls;ready_result=-1;
  assert(!station_online() && ready_calls==notified+1);
  ready_result=0;assert(station_online() && ready_calls==notified+2);
  assert(station_online() && ready_calls==notified+2);
  puts("SDK station: thread ownership, fresh leases, WPA2/AES, failure paths passed");
}
