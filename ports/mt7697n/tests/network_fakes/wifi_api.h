#pragma once
#include <stdint.h>
#define WIFI_PORT_STA 0
#define WIFI_MODE_STA_ONLY 1
#define WIFI_MODE_AP_ONLY 2
#define WIFI_MODE_REPEATER 3
int32_t wifi_config_get_opmode(uint8_t*);
typedef enum {
  WIFI_PHY_11BG_MIXED = 0, WIFI_PHY_11B = 1, WIFI_PHY_11G = 4,
  WIFI_PHY_11N_2_4G = 6, WIFI_PHY_11GN_MIXED = 7, WIFI_PHY_11BGN_MIXED = 9
} wifi_phy_mode_t;
int32_t wifi_config_get_mac_address(uint8_t, uint8_t*);
int32_t wifi_config_get_bssid(uint8_t*);
int32_t wifi_config_get_channel(uint8_t, uint8_t*);
int32_t wifi_config_get_wireless_mode(uint8_t, wifi_phy_mode_t*);
int32_t wifi_config_set_wireless_mode(uint8_t, wifi_phy_mode_t);
typedef enum { WIFI_EVENT_IOT_SCAN_COMPLETE = 1 } wifi_event_t;
typedef int32_t (*wifi_event_handler_t)(wifi_event_t, uint8_t*, uint32_t);
typedef struct {
  uint8_t is_valid;
  int8_t rssi;
  uint8_t ssid[32], ssid_length, bssid[6], channel;
  unsigned auth_mode, encrypt_type;
} wifi_scan_list_item_t;
int32_t wifi_connection_scan_init(wifi_scan_list_item_t*, uint32_t);
int32_t wifi_connection_scan_deinit(void);
int32_t wifi_connection_start_scan(uint8_t*, uint8_t, uint8_t*, uint8_t, uint8_t);
int32_t wifi_connection_stop_scan(void);
int32_t wifi_connection_register_event_handler(wifi_event_t, wifi_event_handler_t);
int32_t wifi_connection_unregister_event_handler(wifi_event_t, wifi_event_handler_t);
