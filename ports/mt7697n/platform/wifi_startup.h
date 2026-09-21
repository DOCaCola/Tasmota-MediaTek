// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
namespace mt7697 {
// Retain manager configuration in RAM, also used by the BSP's first init.
bool prepare_setup_ap(const char* ssid, const char* password, unsigned channel);
// Restore AP settings after set_opmode resets them. Does not reload settings.
bool apply_setup_ap();
}
