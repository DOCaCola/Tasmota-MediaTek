// SPDX-License-Identifier: GPL-3.0-or-later
#include "sdk_network.h"
#include <LWiFi.h>
#include <utility/wifi_drv.h>
#include <string.h>
extern "C" {
#include <wifi_api.h>
}

namespace mt7697 {
namespace {
SdkNetworkDriver station_driver;
Network station_network(station_driver);
}
Network& station() { return station_network; }

bool SdkNetworkDriver::start(const char* ssid, const char* password) {
  // These configure connection without WiFi.begin()'s association wait loop.
  // First call still includes the SDK's synchronous CONNSYS initialization.
  const bool configured = (password[0] == '\0'
      ? WiFiDrv::wifiSetNetwork(ssid, strlen(ssid))
      : WiFiDrv::wifiSetPassphrase(ssid, strlen(ssid), password, strlen(password))) == WL_SUCCESS;
  return configured && wifi_config_set_radio(1) == 0;
}
bool SdkNetworkDriver::stop() {
  // Disconnect-only does nothing while association is pending in this SDK.
  // Radio-off also prevents its automatic connection from defeating backoff.
  return wifi_config_set_radio(0) == 0;
}
bool SdkNetworkDriver::online() {
  return WiFi.status() == WL_CONNECTED && static_cast<uint32_t>(WiFi.localIP()) != 0;
}
}
