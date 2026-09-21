// SPDX-License-Identifier: GPL-3.0-or-later
#include "sdk_network.h"
#include <variant.h>
#include <string.h>
extern "C" {
#include <wifi_api.h>
#include <ethernetif.h>
#include <lwip/netif.h>
#include <lwip/dhcp.h>
#include <lwip/tcpip.h>
#include <lwip/sys.h>
}

namespace mt7697 {
namespace {
SdkNetworkDriver station_driver;
Network station_network(station_driver);
bool started = false;
// Main-task requests are serialized through the TCP/IP mailbox. No Arduino
// station callbacks are registered: their AP/STA event handling is ambiguous.
struct Request { bool link; bool reset; bool online; bool ok; sys_sem_t done; };
void update_interface(void* argument) {
  auto& request = *static_cast<Request*>(argument);
  netif* sta = netif_find_by_type(NETIF_TYPE_STA);
  request.ok = sta != nullptr;
  if (sta) {
    if (request.reset || !request.link) {
      dhcp_stop(sta);
      netif_set_link_down(sta);
      netif_set_addr(sta, IP4_ADDR_ANY, IP4_ADDR_ANY, IP4_ADDR_ANY);
    } else if (!netif_is_link_up(sta)) {
      netif_set_up(sta);
      netif_set_link_up(sta);
      request.ok = dhcp_start(sta) == ERR_OK;
      if (!request.ok) netif_set_link_down(sta);
    }
    request.online = request.link && !request.reset && request.ok &&
        dhcp_supplied_address(sta) && !ip4_addr_isany_val(sta->ip_addr);
    // The directly connected AP subnet retains its route; external traffic
    // uses the station only after DHCP establishes its gateway.
    if (request.online) netif_set_default(sta);
  }
  sys_sem_signal(&request.done);
}
bool interface_request(bool link, bool reset, bool* online = nullptr) {
  Request request{link, reset, false, false, {}};
  if (sys_sem_new(&request.done, 0) != ERR_OK) return false;
  if (tcpip_callback(update_interface, &request) != ERR_OK) {
    sys_sem_free(&request.done);
    return false;
  }
  sys_arch_sem_wait(&request.done, 0);
  sys_sem_free(&request.done);
  if (online) *online = request.online;
  return request.ok;
}
}
Network& station() { return station_network; }

bool station_start(const char* ssid, const char* password) {
  const bool initialized = wifi_ready();
  started = false;
  init_global_connsys();
  // Disconnect the station without cycling the shared radio. In this SDK the
  // disconnect implementation explicitly handles modes 1 and 3; set_radio
  // rejects mode 3. A fresh initialization has no association to disconnect.
  if ((initialized && wifi_connection_disconnect_ap() < 0) ||
      !interface_request(false, true)) return false;
  const bool secured = password[0] != '\0';
  if (wifi_config_set_ssid(WIFI_PORT_STA,
          reinterpret_cast<uint8_t*>(const_cast<char*>(ssid)), strlen(ssid)) < 0 ||
      wifi_config_set_security_mode(WIFI_PORT_STA,
          secured ? WIFI_AUTH_MODE_WPA2_PSK : WIFI_AUTH_MODE_OPEN,
          secured ? WIFI_ENCRYPT_TYPE_AES_ENABLED : WIFI_ENCRYPT_TYPE_WEP_DISABLED) < 0 ||
      (secured && wifi_config_set_wpa_psk_key(WIFI_PORT_STA,
          reinterpret_cast<uint8_t*>(const_cast<char*>(password)), strlen(password)) < 0) ||
      wifi_config_reload_setting() < 0) return false;
  started = true;
  return true;
}
bool station_stop() {
  started = false;
  if (!wifi_ready()) return true;
  const bool disconnected = wifi_connection_disconnect_ap() >= 0;
  return interface_request(false, true) && disconnected;
}
bool station_online() {
  if (!started) return false;
  uint8_t link = 0;
  const bool connected = wifi_connection_get_link_status(&link) >= 0 &&
      link == WIFI_STATUS_LINK_CONNECTED;
  bool online = false;
  return interface_request(connected, false, &online) && online;
}
bool SdkNetworkDriver::start(const char* ssid, const char* password) {
  return station_start(ssid, password);
}
bool SdkNetworkDriver::stop() { return station_stop(); }
bool SdkNetworkDriver::online() { return station_online(); }
}
