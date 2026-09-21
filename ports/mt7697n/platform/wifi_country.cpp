// SPDX-License-Identifier: GPL-3.0-or-later
#include "../port_config.h"
#include "wifi_startup.h"
#include <string.h>
extern "C" {
#include <wifi_api.h>

}
namespace {
wifi_ap_config_t startup_ap;
bool startup_ap_pending=false;
}
namespace mt7697 {
bool prepare_setup_ap(const char* ssid,const char* password,unsigned channel) {
  const size_t sl=strlen(ssid),pl=strlen(password);
  if (!sl || sl>32 || (pl && (pl<8 || pl>63)) || channel<1 || channel>13) return false;
  for (size_t i=0;i<pl;++i) if (static_cast<unsigned char>(password[i])<32 || static_cast<unsigned char>(password[i])>126) return false;
  startup_ap={};
  memcpy(startup_ap.ssid,ssid,sl);startup_ap.ssid_length=sl;
  memcpy(startup_ap.password,password,pl);startup_ap.password_length=pl;
  startup_ap.auth_mode=pl?WIFI_AUTH_MODE_WPA2_PSK:WIFI_AUTH_MODE_OPEN;
  startup_ap.encrypt_type=pl?WIFI_ENCRYPT_TYPE_AES_ENABLED:WIFI_ENCRYPT_TYPE_WEP_DISABLED;
  startup_ap.channel=channel;
  startup_ap.bandwidth=WIFI_IOT_COMMAND_CONFIG_BANDWIDTH_20MHZ;
  startup_ap_pending=true;
  return true;
}
bool apply_setup_ap() {
  return wifi_config_set_ssid(WIFI_PORT_AP,startup_ap.ssid,startup_ap.ssid_length)>=0 &&
      wifi_config_set_channel(WIFI_PORT_AP,startup_ap.channel)>=0 &&
      wifi_config_set_security_mode(WIFI_PORT_AP,startup_ap.auth_mode,startup_ap.encrypt_type)>=0 &&
      (!startup_ap.password_length ||
       wifi_config_set_wpa_psk_key(WIFI_PORT_AP,startup_ap.password,startup_ap.password_length)>=0);
}
}
extern "C" {
void __real_wifi_init(wifi_config_t* config, wifi_config_ext_t* extended);

// Linker wrapping intercepts the unmodified BSP's initialization call.
// Preserve all other extended options (notably disabled auto-connect).
void __wrap_wifi_init(wifi_config_t* config, wifi_config_ext_t* extended) {
  wifi_config_ext_t native = {};
  if (extended) native = *extended;
  native.country_code_present = 1;
  native.country_code[0] = mt7697::wifi_country[0];
  native.country_code[1] = mt7697::wifi_country[1];
  native.country_code[2] = native.country_code[3] = 0;
  if (startup_ap_pending) {
    // Update the caller's mode too: the BSP passes it to lwip_tcpip_init next.
    config->opmode=WIFI_MODE_AP_ONLY;
    config->ap_config=startup_ap;
    native.ap_hidden_ssid_enable_present=1;
    native.ap_hidden_ssid_enable=0;
    startup_ap_pending=false;
  }
  __real_wifi_init(config, &native);
}
}
