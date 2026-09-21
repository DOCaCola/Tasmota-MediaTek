// SPDX-License-Identifier: GPL-3.0-or-later
#ifdef TASMOTA_PLATFORM_MT7697N
extern "C" {
#include <wifi_api.h>
#include <lwip/api.h>
}

String WifiMacAddress(void) {
  uint8_t mac[6];
  if (wifi_config_get_mac_address(WIFI_PORT_STA, mac) < 0) {
    AddLog(LOG_LEVEL_ERROR, PSTR("WIF: Cannot read station MAC"));
    return String();
  }
  char text[18];
  snprintf(text, sizeof(text), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(text);
}

uint32_t ESP_getChipId(void) {
  uint8_t mac[6];
  if (wifi_config_get_mac_address(WIFI_PORT_STA, mac) < 0) {
    AddLog(LOG_LEVEL_ERROR, PSTR("WIF: Station identity unavailable; stopping startup"));
    abort(); // Do not create shared MQTT identities from a fabricated chip ID.
  }
  return (uint32_t(mac[3]) << 16) | (uint32_t(mac[4]) << 8) | mac[5];
}

bool WifiHasIP(void) {
  return WiFi.status() == WL_CONNECTED && static_cast<uint32_t>(WiFi.localIP()) != 0;
}

String IPGetListeningAddressStr(void) {
  return WifiHasIP() ? WiFi.localIP().toString() : String();
}

bool WifiHostByName(const char* hostname, IPAddress& result) {
  IPAddress parsed;
  if (!parsed.fromString(hostname)) {
    if (!WifiHasIP()) { return false; }
    ip_addr_t address;
    // netconn marshals DNS work into the lwIP task and owns each request.
    // The BSP WiFiDrv resolver instead shares one mutable global result.
    // This uses lwIP's DNS retry timeout, not Settings->dns_timeout.
    if (netconn_gethostbyname(hostname, &address) != ERR_OK) {
      AddLog(LOG_LEVEL_DEBUG, PSTR("WIF: DNS failed for %s"), hostname);
      return false;
    }
    parsed = address.addr;
  }
  const uint32_t value = static_cast<uint32_t>(parsed);
  if (!value || value == UINT32_MAX) { return false; }
  result = parsed;
  return true;
}

String WifiDowntime(void) { return GetDuration(Wifi.downtime); }

String WifiBssid(void) {
  uint8_t mac[6];
  if (wifi_config_get_bssid(mac) < 0) { return String(); }
  char text[18];
  snprintf(text, sizeof(text), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(text);
}

int WifiChannel(void) {
  uint8_t channel;
  return wifi_config_get_channel(WIFI_PORT_STA, &channel) < 0 ? -1 : channel;
}

String WifiGetPhyMode(void) {
  wifi_phy_mode_t mode;
  if (wifi_config_get_wireless_mode(WIFI_PORT_STA, &mode) < 0) {
    return String("Unknown");
  }
  switch (mode) {
    case WIFI_PHY_11B: return String("11b");
    case WIFI_PHY_11G: return String("11g");
    case WIFI_PHY_11BG_MIXED: return String("11bg");
    case WIFI_PHY_11N_2_4G: return String("11n");
    case WIFI_PHY_11GN_MIXED: return String("11gn");
    case WIFI_PHY_11BGN_MIXED: return String("11bgn");
    default: return String("Unknown");
  }
}
#endif
