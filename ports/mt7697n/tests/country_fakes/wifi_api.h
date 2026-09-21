#pragma once
#include <stdint.h>
typedef struct { unsigned opmode; } wifi_config_t;
typedef struct {
  unsigned country_code_present, sta_auto_connect_present, sta_auto_connect;
  uint8_t country_code[4];
} wifi_config_ext_t;
