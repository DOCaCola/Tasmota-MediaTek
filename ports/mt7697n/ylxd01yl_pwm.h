// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <stdint.h>

namespace ylxd01yl {
constexpr uint32_t kFrequencyHz = 10000;
constexpr uint32_t kPeriodCounts = 4000;

// Counts are calibrated hardware duties, not brightness or color temperature.
// The caller must apply the stock calibration before using this backend.
class Pwm {
 public:
  bool begin();
  bool daylight(uint32_t warm, uint32_t cold);
  bool night(uint32_t duty);
  bool off();

 private:
  bool apply(uint32_t warm, uint32_t cold, uint32_t night);
  bool shutdown();
  bool ready_ = false;
  uint32_t duties_[3] = {0, 0, 0};
};
}  // namespace ylxd01yl
