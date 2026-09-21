// SPDX-License-Identifier: GPL-3.0-or-later
#include <cstdint>
#include <cassert>
#include <cstdio>
#define PROGMEM
#define PSTR(x) x
#define TASMOTA_PLATFORM_MT7697N
#include "include/tasmota.h"
#include "language/en_GB.h"
#include "include/tasmota_template.h"
#include "include/tasmota_types.h"
#include "../platform/network.h"

TSettings settings{};
TSettings* Settings = &settings;
struct {
  struct { bool wifi_down = true, network_down = true; } global_state;
  struct { bool wifi_connected = false, wifi_disconnected = false; } rules_flag;
} TasmotaGlobal;
struct { uint16_t link_count = 0; uint32_t downtime = 0, last_event = 0; uint8_t status = 0; } Wifi;
uint32_t now = 0;
int errors = 0;
bool configured = true, bssid_set = false;
void AddLog(int level, const char*, ...) { if (level == LOG_LEVEL_ERROR) { ++errors; } }
uint32_t millis() { return now; }
uint32_t UpTime() { return now / 1000; }
const char* SettingsText(uint32_t index) {
  if (index == SET_STASSID1) { return configured ? "lamp-network" : ""; }
  if (index == SET_STAPWD1) { return "password"; }
  if (index == SET_APBSSID1) { return bssid_set ? "00:11:22:33:44:55" : ""; }
  return "";
}
struct Driver : mt7697::NetworkDriver {
  bool connected = false, stops_ok = true;
  int starts = 0;
  bool start(const char*, const char*) override { ++starts; return true; }
  bool stop() override { connected = false; return stops_ok; }
  bool online() override { return connected; }
} driver;
namespace mt7697 {
Network native_network(driver);
Network& station() { return native_network; }
}

#include "tasmota_support/support_wifi_mt7697.ino"

int main() {
  WifiConnect();
  assert(driver.starts == 0);  // Disabled setting is respected.
  Settings->flag4.network_wifi = true;
  configured = false;
  WifiConnect();
  assert(driver.starts == 0);
  configured = true;
  Settings->ipv4_address[0] = 1;
  WifiConnect();
  assert(driver.starts == 0 && errors == 1);
  Settings->ipv4_address[0] = 0;
  bssid_set = true;
  WifiConnect();
  assert(driver.starts == 0 && errors == 2);
  bssid_set = false;
  WifiConnect();
  assert(driver.starts == 1 && WifiState() == -1);
  now = 5000;
  driver.connected = true;
  WifiCheck(WIFI_RESTART);
  assert(WifiState() == WIFI_RESTART && Wifi.status == WL_CONNECTED);
  assert(!TasmotaGlobal.global_state.network_down && Wifi.link_count == 1 && Wifi.downtime == 5);
  assert(TasmotaGlobal.rules_flag.wifi_connected);
  WifiCheck(WIFI_RESTART);
  assert(Wifi.link_count == 1);  // No repeated connection event.
  now = 6000;
  driver.connected = false;
  WifiCheck(WIFI_RESTART);
  assert(TasmotaGlobal.global_state.network_down && TasmotaGlobal.rules_flag.wifi_disconnected);
  now = 11000;
  WifiCheck(WIFI_RESTART);
  assert(driver.starts == 2);
  now = 12000;
  driver.connected = true;
  WifiCheck(WIFI_RESTART);
  assert(WifiLinkCount() == 2 && Wifi.downtime == 11);
  Settings->flag4.network_wifi = false;
  WifiCheck(WIFI_RESTART);
  assert(mt7697::station().state() == mt7697::NetworkState::Disabled);
  assert(TasmotaGlobal.global_state.network_down);
  Settings->flag4.network_wifi = true;
  WifiEnable();
  driver.stops_ok = false;
  WifiDisable();
  assert(mt7697::station().state() == mt7697::NetworkState::Fault);
  assert(WifiState() == -1);
  assert(WifiGetRssiAsQuality(-101) == 0 && WifiGetRssiAsQuality(-75) == 50);
  puts("Core network tests passed: native lifecycle, Tasmota flags/events, downtime, unsupported settings and stop failure.");
}
