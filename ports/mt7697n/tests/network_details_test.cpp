// SPDX-License-Identifier: GPL-3.0-or-later
#include "../arduino/IPAddress.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#define TASMOTA_PLATFORM_MT7697N
#define LOG_LEVEL_ERROR 1
#define LOG_LEVEL_DEBUG 4
#define WL_CONNECTED 3
static bool online = true;
namespace mt7697 { bool station_online() { return online; } }
static int api_result = 0, dns_result = 0, dns_calls = 0;
static uint32_t dns_address;
static int last_phy = -1;
struct {
  int status() { return online ? WL_CONNECTED : 0; }
  IPAddress localIP() { return IPAddress(192, 0, 2, 1); }
} WiFi;
struct { uint32_t downtime = 12; } Wifi;
String GetDuration(uint32_t seconds) { return String(seconds); }
void AddLog(unsigned, const char*, ...) {}
#include "../../../tasmota/tasmota_support/support_network_mt7697.ino"

extern "C" {
int32_t wifi_config_get_mac_address(uint8_t port, uint8_t* mac) {
  assert(port == WIFI_PORT_STA);
  const uint8_t value[] = {2, 0x11, 0x22, 0x33, 0x44, 0x55};
  memcpy(mac, value, sizeof(value));
  return api_result;
}
int32_t wifi_config_get_bssid(uint8_t* mac) {
  return wifi_config_get_mac_address(WIFI_PORT_STA, mac);
}
int32_t wifi_config_get_channel(uint8_t, uint8_t* channel) {
  *channel = 6; return api_result;
}
int32_t wifi_config_get_wireless_mode(uint8_t, wifi_phy_mode_t* mode) {
  *mode = WIFI_PHY_11BGN_MIXED; return api_result;
}
int32_t wifi_config_set_wireless_mode(uint8_t port, wifi_phy_mode_t mode) {
  assert(port == WIFI_PORT_STA);
  last_phy = mode;
  return api_result;
}
int netconn_gethostbyname(const char*, ip_addr_t* address) {
  ++dns_calls; address->addr = dns_address; return dns_result;
}
}
int main() {
  assert(WifiMacAddress() == "02:11:22:33:44:55");
  assert(ESP_getChipId() == 0x334455);
  assert(WifiBssid() == "02:11:22:33:44:55");
  assert(WifiChannel() == 6);
  assert(WifiGetPhyMode() == "11bgn");
  assert(WifiSetPhyMode(1) && last_phy == WIFI_PHY_11B);
  assert(WifiSetPhyMode(2) && last_phy == WIFI_PHY_11BG_MIXED);
  assert(WifiSetPhyMode(3) && last_phy == WIFI_PHY_11BGN_MIXED);
  assert(!WifiSetPhyMode(4) && last_phy == WIFI_PHY_11BGN_MIXED);
  assert(IPGetListeningAddressStr() == "192.0.2.1");
  IPAddress result(198, 51, 100, 2);
  const IPAddress original = result;
  dns_result = -1;
  assert(!WifiHostByName("missing.example", result));
  assert(result == original);
  dns_result = 0;
  dns_address = UINT32_MAX;
  assert(!WifiHostByName("broadcast.example", result));
  assert(result == original);
  dns_address = uint32_t(IPAddress(203, 0, 113, 9));
  assert(WifiHostByName("broker.example", result));
  assert(uint32_t(result) == dns_address);
  online = false;
  const int calls = dns_calls;
  assert(!WifiHostByName("broker.example", result));
  assert(dns_calls == calls);
  assert(WifiHostByName("203.0.113.10", result));
  assert(result == IPAddress(203, 0, 113, 10));
  assert(!WifiHostByName("0.0.0.0", result));
  assert(result == IPAddress(203, 0, 113, 10));
  assert(IPGetListeningAddressStr() == "");
  api_result = -1;
  assert(!WifiSetPhyMode(1));
  assert(WifiMacAddress() == "");
  assert(WifiBssid() == "");
  assert(WifiChannel() == -1);
  assert(WifiGetPhyMode() == "Unknown");
  puts("Network details tests passed: MAC order/identity, DNS failure preservation, offline behavior, metadata errors.");
}
