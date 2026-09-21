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
static wifi_ap_config_t applied;
static int calls=0,fail_call=0;
int wifi_config_set_ssid(uint8_t port,uint8_t* ssid,uint8_t length) {
  assert(port==WIFI_PORT_AP);memcpy(applied.ssid,ssid,length);applied.ssid_length=length;
  return ++calls==fail_call?-1:0;
}
int wifi_config_set_channel(uint8_t port,uint8_t channel) {
  assert(port==WIFI_PORT_AP);applied.channel=channel;return ++calls==fail_call?-1:0;
}
int wifi_config_set_security_mode(uint8_t port,unsigned auth,unsigned encryption) {
  assert(port==WIFI_PORT_AP);applied.auth_mode=auth;applied.encrypt_type=encryption;
  return ++calls==fail_call?-1:0;
}
int wifi_config_set_wpa_psk_key(uint8_t port,uint8_t* password,uint8_t length) {
  assert(port==WIFI_PORT_AP);memcpy(applied.password,password,length);applied.password_length=length;
  return ++calls==fail_call?-1:0;
}
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
  // Mode resets must restore the full AP identity/security after first init.
  assert(mt7697::apply_setup_ap());
  assert(applied.ssid_length==6 && !memcmp(applied.ssid,"secure",6));
  assert(applied.channel==1 && applied.auth_mode==WIFI_AUTH_MODE_WPA2_PSK);
  assert(applied.encrypt_type==WIFI_ENCRYPT_TYPE_AES_ENABLED);
  assert(applied.password_length==8 && !memcmp(applied.password,"password",8));
  for(int i=1;i<=4;++i) {
    calls=0;fail_call=i;assert(!mt7697::apply_setup_ap() && calls==i);
  }
  fail_call=0;calls=0;
  assert(mt7697::prepare_setup_ap("open","",6) && mt7697::apply_setup_ap());
  assert(calls==3 && applied.auth_mode==WIFI_AUTH_MODE_OPEN && applied.channel==6);
  puts("Wi-Fi country tests passed: override before SDK init, other options preserved");
}
