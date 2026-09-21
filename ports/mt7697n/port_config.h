// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// Radio country passed to the MediaTek SDK before Wi-Fi starts.
// Two uppercase letters supported by the SDK, e.g. "DE" (Germany).
// This is independent of Tasmota language/timezone and does not write eFuses.
#ifndef MT7697_WIFI_COUNTRY
#define MT7697_WIFI_COUNTRY "DE"
#endif

namespace mt7697 {
constexpr char wifi_country[] = MT7697_WIFI_COUNTRY;
static_assert(sizeof(wifi_country) == 3 &&
              wifi_country[0] >= 'A' && wifi_country[0] <= 'Z' &&
              wifi_country[1] >= 'A' && wifi_country[1] <= 'Z',
              "MT7697_WIFI_COUNTRY must contain two uppercase country letters");
}
