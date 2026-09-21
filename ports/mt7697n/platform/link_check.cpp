// SPDX-License-Identifier: GPL-3.0-or-later
#include "sdk_network.h"
#include "settings.h"
#include "system.h"

// Link coverage only. Never called by a probe. Successful linking is not a
// hardware test, and probe startup must not initialize or write NVDM.
extern "C" bool mt7697_platform_link_check(void* settings, uint32_t now) {
  if (now == UINT32_MAX) { mt7697::restart(); }
  const bool system_valid = mt7697::image_size() <= mt7697::application_capacity() &&
      mt7697::minimum_free_heap() <= mt7697::free_heap() &&
      mt7697::stack_low_water_bytes() != 0 &&
      mt7697::reset_reason_text()[0] != '\0';
  mt7697::SdkNetworkDriver driver;
  mt7697::Network network(driver);
  bool result = network.begin("link-check", "", now);
  network.poll(now);
  result = network.stop() && result;
  return result && system_valid &&
      mt7697::sdk_load_settings(settings, mt7697::kSettingsSize) == mt7697::StorageResult::Ok &&
      mt7697::sdk_save_settings(settings, mt7697::kSettingsSize) == mt7697::StorageResult::Ok;
}
