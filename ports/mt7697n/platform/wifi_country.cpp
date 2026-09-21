// SPDX-License-Identifier: GPL-3.0-or-later
#include "../port_config.h"
extern "C" {
#include <wifi_api.h>

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
  __real_wifi_init(config, &native);
}
}
