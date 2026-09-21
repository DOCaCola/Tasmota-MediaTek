/*
  Native MT7697N station lifecycle for Tasmota.
  SPDX-License-Identifier: GPL-3.0-or-later
*/
#ifdef TASMOTA_PLATFORM_MT7697N
#include <platform/network.h>
#include <utility/wl_definitions.h>

void WifiSetState(uint8_t state) {
  if (state == TasmotaGlobal.global_state.wifi_down) {
    if (state) {
      TasmotaGlobal.rules_flag.wifi_connected = 1;
      Wifi.link_count++;
      Wifi.downtime += UpTime() - Wifi.last_event;
    } else {
      TasmotaGlobal.rules_flag.wifi_disconnected = 1;
      Wifi.last_event = UpTime();
    }
  }
  TasmotaGlobal.global_state.wifi_down = state ^ 1;
  // This target currently exposes only its station network interface.
  TasmotaGlobal.global_state.network_down = state ^ 1;
}

void WifiShutdown(bool option) {
#ifdef USE_WEBSERVER
  NativeWifiManagerStop();
#endif
  // The ESP "erase SDK credentials" option is inapplicable: Tasmota owns them.
  (void)option;
  if (!mt7697::station().stop()) {
    AddLog(LOG_LEVEL_ERROR, PSTR("WIF: Native radio stop failed"));
  }
  Wifi.status = WL_DISCONNECTED;
  WifiSetState(0);  // Faulted or disabled networking cannot service MQTT.
}

void WifiDisable(void) { WifiShutdown(false); }

void WifiConnect(void) {
  WifiShutdown(false);
  if (!Settings->flag4.network_wifi) { return; }
  if (mt7697::station().state() == mt7697::NetworkState::Fault) { return; }
  if (Settings->sta_active >= MAX_SSIDS || Settings->ipv4_address[0] != 0) {
    AddLog(LOG_LEVEL_ERROR, PSTR("WIF: Native station requires a valid SSID slot and DHCP"));
    return;
  }
  const char* ssid = SettingsText(SET_STASSID1 + Settings->sta_active);
  const char* password = SettingsText(SET_STAPWD1 + Settings->sta_active);
  if (SettingsText(SET_APBSSID1 + Settings->sta_active)[0]) {
    AddLog(LOG_LEVEL_ERROR, PSTR("WIF: BSSID pinning is not implemented on MT7697N"));
    return;
  }
  if (!ssid[0]) {
#ifdef USE_WEBSERVER
    WifiManagerBegin(false);
#else
    AddLog(LOG_LEVEL_INFO, PSTR("WIF: Configure SSID and password through the serial commands"));
#endif
    return;
  }
  if (!mt7697::station().begin(ssid, password, millis())) {
    AddLog(LOG_LEVEL_ERROR, PSTR("WIF: Native station start failed (%u)"),
           static_cast<unsigned>(mt7697::station().error()));
  }
}

void WifiCheckIp(void) {
#ifdef USE_WEBSERVER
  if (NativeWifiManagerActive()) {
    WifiSetState(WifiHasIP() ? 1 : 0);
    return;
  }
#endif
  mt7697::station().poll(millis());
  const bool online = mt7697::station().state() == mt7697::NetworkState::Online;
  Wifi.status = online ? WL_CONNECTED : WL_DISCONNECTED;
  WifiSetState(online ? 1 : 0);
}

void WifiCheck(uint8_t param) {
  if (!Settings->flag4.network_wifi) {
    WifiDisable();
    return;
  }
#ifdef USE_WEBSERVER
  if (param == WIFI_MANAGER || param == WIFI_MANAGER_RESET_ONLY) {
    if (!NativeWifiManagerActive()) {
      WifiShutdown(false);
      WifiManagerBegin(param == WIFI_MANAGER_RESET_ONLY);
    }
    NativeWifiManagerPoll();
    WifiCheckIp();
    return;
  }
  NativeWifiManagerPoll();
#endif
  if (param != WIFI_RESTART && param != WIFI_RETRY && param != WIFI_WAIT) {
    AddLog(LOG_LEVEL_ERROR, PSTR("WIF: Configuration mode %u is not implemented on MT7697N"), param);
  }
  WifiCheckIp();
}

void WifiEnable(void) { WifiConnect(); }
uint16_t WifiLinkCount(void) { return Wifi.link_count; }
int WifiState(void) {
#ifdef USE_WEBSERVER
  if (NativeWifiManagerActive()) return WIFI_MANAGER;
#endif
  return TasmotaGlobal.global_state.wifi_down ? -1 : WIFI_RESTART;
}
int WifiGetRssiAsQuality(int rssi) {
  return rssi <= -100 ? 0 : rssi >= -50 ? 100 : 2 * (rssi + 100);
}
#endif
