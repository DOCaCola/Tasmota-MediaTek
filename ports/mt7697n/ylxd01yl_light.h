// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <stdint.h>
#include "ylxd01yl_pwm.h"

namespace ylxd01yl {
constexpr unsigned kColdMired = 153;
constexpr unsigned kWarmMired = 370;
struct Duties { uint32_t warm, cold, night; };
struct Calibration { unsigned kelvin, warm, cold; }; // hundredths of a percent
// Stock 1.5.9_0189 table at 0x1012be60. Piecewise linear interpolation
// deliberately avoids overshoot; it is not the stock three-point algorithm.
constexpr Calibration calibration[] = {
  {2700,10000,0}, {3000,7911,999}, {3200,7162,1689},
  {3400,6526,2348}, {4000,4853,4114}, {4500,3599,5324},
  {5000,2475,6348}, {5500,1579,7182}, {5800,1335,7422},
  {6000,800,8091}, {6500,0,10000}
};

inline Duties daylight(unsigned kelvin, unsigned brightness) {
  if (brightness > 1023) brightness = 1023;
  if (kelvin < 2700) kelvin = 2700;
  if (kelvin > 6500) kelvin = 6500;
  unsigned i = 0;
  while (i + 1 < sizeof(calibration)/sizeof(calibration[0]) - 1 &&
         kelvin > calibration[i+1].kelvin) ++i;
  const auto& a = calibration[i];
  const auto& b = calibration[i+1];
  const unsigned span = b.kelvin-a.kelvin, offset = kelvin-a.kelvin;
  const uint32_t warm = (a.warm*(span-offset)+b.warm*offset)/span;
  const uint32_t cold = (a.cold*(span-offset)+b.cold*offset)/span;
  // Floor both channels, preserving the combined 4000-count bound.
  return {uint32_t(uint64_t(warm)*brightness*kPeriodCounts/(10000u*1023u)),
          uint32_t(uint64_t(cold)*brightness*kPeriodCounts/(10000u*1023u)),0};
}

// Input is the current faded/gamma-corrected Tasmota CCT frame, cold first.
// Derive temperature from this frame, not from the final requested CT.
inline Duties frame(unsigned cold, unsigned warm, bool night) {
  if (cold > 1023) cold = 1023;
  if (warm > 1023) warm = 1023;
  const unsigned total = cold+warm;
  if (!total) return {0,0,0};
  const unsigned brightness = total > 1023 ? 1023 : total;
  if (night) return {0,0,brightness*kPeriodCounts/1023};
  if (!warm) return daylight(6500,brightness);
  if (!cold) return daylight(2700,brightness);
  const unsigned mired = kColdMired +
      ((kWarmMired-kColdMired)*warm + total/2)/total;
  return daylight(1000000u/mired,brightness);
}
}
