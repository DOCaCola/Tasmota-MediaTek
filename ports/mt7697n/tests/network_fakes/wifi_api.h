#pragma once
#include <stdint.h>
#define WIFI_PORT_STA 0
typedef enum {
  WIFI_PHY_11BG_MIXED = 0, WIFI_PHY_11B = 1, WIFI_PHY_11G = 4,
  WIFI_PHY_11N_2_4G = 6, WIFI_PHY_11GN_MIXED = 7, WIFI_PHY_11BGN_MIXED = 9
} wifi_phy_mode_t;
int32_t wifi_config_get_mac_address(uint8_t, uint8_t*);
int32_t wifi_config_get_bssid(uint8_t*);
int32_t wifi_config_get_channel(uint8_t, uint8_t*);
int32_t wifi_config_get_wireless_mode(uint8_t, wifi_phy_mode_t*);
