#pragma once
#include <stdint.h>
enum {WIFI_MODE_AP_ONLY=2,WIFI_MODE_REPEATER=3,WIFI_AUTH_MODE_OPEN=0,WIFI_AUTH_MODE_WPA2_PSK=7,WIFI_ENCRYPT_TYPE_AES_ENABLED=3,WIFI_ENCRYPT_TYPE_WEP_DISABLED=1,WIFI_IOT_COMMAND_CONFIG_BANDWIDTH_20MHZ=0};
typedef struct { uint8_t ssid[32],ssid_length,password[64],password_length; unsigned auth_mode,encrypt_type;uint8_t channel,bandwidth; } wifi_ap_config_t;
typedef struct { unsigned opmode; wifi_ap_config_t ap_config; } wifi_config_t;
typedef struct {
  unsigned country_code_present, sta_auto_connect_present, sta_auto_connect;
  uint8_t country_code[4];
  unsigned ap_hidden_ssid_enable_present,ap_hidden_ssid_enable;
} wifi_config_ext_t;
