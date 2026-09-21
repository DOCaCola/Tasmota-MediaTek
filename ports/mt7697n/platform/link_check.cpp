// SPDX-License-Identifier: GPL-3.0-or-later
#include "sdk_network.h"
#include "settings.h"

// Link coverage only. Never called by a probe. Successful linking is not a
// hardware test, and probe startup must not initialize or write NVDM.
extern "C" bool mt7697_platform_link_check(void* settings, uint32_t now) {
  mt7697::SdkNetworkDriver driver;
  mt7697::Network network(driver);
  bool result = network.begin("link-check", "", now);
  network.poll(now);
  result = network.stop() && result;
  return result &&
      mt7697::sdk_load_settings(settings, mt7697::kSettingsSize) == mt7697::StorageResult::Ok &&
      mt7697::sdk_save_settings(settings, mt7697::kSettingsSize) == mt7697::StorageResult::Ok;
}
