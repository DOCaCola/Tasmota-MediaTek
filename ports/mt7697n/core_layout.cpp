// SPDX-License-Identifier: GPL-3.0-or-later
// Compile the actual core settings definition, not a stand-in structure.
#include <Arduino.h>
#include <stddef.h>
#include "include/tasmota.h"
#include "language/en_GB.h"
#include "include/tasmota_template.h"
#include "include/tasmota_types.h"
#include "platform/settings.h"

static_assert(sizeof(TSettings) == mt7697::kSettingsSize, "Native settings size");
static_assert(offsetof(TSettings, my_gp) == 0x3ac, "Native GPIO offset");
static_assert(offsetof(TSettings, user_template) == 0x3fc, "Native template offset");
static_assert(offsetof(TSettings, lamp_config_version) == 0x404, "Native lamp offset");
static_assert(offsetof(TSettings, serial_delimiter) == 0x451, "Serial settings offset");
static_assert(offsetof(TSettings, cfg_crc32) == 0xffc, "Settings CRC offset");
