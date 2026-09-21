// SPDX-License-Identifier: GPL-3.0-or-later
#include "../port_config.h"
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
  wifi_config_t config = {1};
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
  puts("Wi-Fi country tests passed: override before SDK init, other options preserved");
}
