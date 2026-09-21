// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "network.h"

namespace mt7697 {
class SdkNetworkDriver final : public NetworkDriver {
 public:
  bool start(const char* ssid, const char* password) override;
  bool stop() override;
  bool online() override;
};
}
