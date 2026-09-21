// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
namespace mt7697 {
// Called before the BSP's first init_global_connsys(). One-shot RAM settings.
bool prepare_setup_ap(const char* ssid, const char* password, unsigned channel);
}
