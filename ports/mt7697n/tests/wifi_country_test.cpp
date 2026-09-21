// SPDX-License-Identifier: GPL-3.0-or-later
#include "../port_config.h"
#include "../platform/wifi_startup.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
extern "C" {
#include <wifi_api.h>
void __wrap_wifi_init(wifi_config_t*, wifi_config_ext_t*);
static wifi_config_t* captured_config;
static wifi_config_ext_t captured;
void __real_wifi_init(wifi_config_t* config, wifi_config_ext_t* extended) {
  captured_config = config;
  captured = *extended;
}
}
int main() {
  wifi_config_t config = {};
  config.opmode=1;
  wifi_config_ext_t extended = {};
  extended.sta_auto_connect_present = 1;
  extended.sta_auto_connect = 0;
  extended.country_code_present = 1;
  memcpy(extended.country_code, "TW", 3);
  const auto original = extended;
  __wrap_wifi_init(&config, &extended);
  assert(captured_config == &config && captured.country_code_present == 1);
  assert(!memcmp(captured.country_code, mt7697::wifi_country, 3));
  assert(captured.country_code[3] == 0);
  assert(captured.sta_auto_connect_present == 1 && captured.sta_auto_connect == 0);
  assert(!memcmp(&original, &extended, sizeof(extended)));
  __wrap_wifi_init(&config, nullptr);
  assert(captured.country_code_present == 1 && !captured.sta_auto_connect_present);
  assert(!memcmp(captured.country_code, mt7697::wifi_country, 3));
  assert(!mt7697::prepare_setup_ap("", "", 1));
  assert(!mt7697::prepare_setup_ap("lamp", "short", 1));
  assert(!mt7697::prepare_setup_ap("lamp", "", 14));
  assert(mt7697::prepare_setup_ap("tasmota-test", "", 6));
  __wrap_wifi_init(&config, &extended);
  assert(config.opmode==WIFI_MODE_AP_ONLY && config.ap_config.channel==6);
  assert(config.ap_config.ssid_length==12 && !memcmp(config.ap_config.ssid,"tasmota-test",12));
  assert(config.ap_config.auth_mode==WIFI_AUTH_MODE_OPEN && !config.ap_config.password_length);
  assert(captured.ap_hidden_ssid_enable_present && !captured.ap_hidden_ssid_enable);
  assert(captured.sta_auto_connect==0 && !memcmp(captured.country_code,"DE",3));
  config={};config.opmode=1;__wrap_wifi_init(&config,&extended);
  assert(config.opmode==1); // One-shot AP settings never affect later station init.
  assert(mt7697::prepare_setup_ap("secure", "password", 1));
  __wrap_wifi_init(&config,&extended);
  assert(config.ap_config.auth_mode==WIFI_AUTH_MODE_WPA2_PSK && config.ap_config.password_length==8);
  puts("Wi-Fi country tests passed: override before SDK init, other options preserved");
}
