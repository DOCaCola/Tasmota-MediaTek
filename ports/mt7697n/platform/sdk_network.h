// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "network.h"

namespace mt7697 {
bool station_start(const char* ssid, const char* password);
bool station_stop();
bool station_online();
class SdkNetworkDriver final : public NetworkDriver {
 public:
  bool start(const char* ssid, const char* password) override;
  bool stop() override;
  bool online() override;
};
}
