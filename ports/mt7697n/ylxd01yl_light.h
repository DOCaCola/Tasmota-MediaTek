// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <stdint.h>
#include "ylxd01yl_pwm.h"

#pragma GCC push_options
#pragma GCC optimize ("fp-contract=off")
namespace ylxd01yl {
constexpr unsigned kColdMired = 153;
constexpr unsigned kWarmMired = 370;
struct Duties { uint32_t warm, cold, night; };
struct Calibration { unsigned kelvin, warm, cold; }; // hundredths of a percent
// Stock 1.5.9_0189 table at 0x1012be60.
constexpr Calibration calibration[] = {
  {2700,10000,0}, {3000,7911,999}, {3200,7162,1689},
  {3400,6526,2348}, {4000,4853,4114}, {4500,3599,5324},
  {5000,2475,6348}, {5500,1579,7182}, {5800,1335,7422},
  {6000,800,8091}, {6500,0,10000}
};

inline float interpolate(float x, const Calibration& a, const Calibration& b,
                         const Calibration& c, bool warm) {
  const float x1=a.kelvin, x2=b.kelvin, x3=c.kelvin;
  const float y1=(warm?a.warm:a.cold)/100.0f;
  const float y2=(warm?b.warm:b.cold)/100.0f;
  const float y3=(warm?c.warm:c.cold)/100.0f;
  return y1*(x-x2)*(x-x3)/((x1-x2)*(x1-x3)) +
         y2*(x-x3)*(x-x1)/((x2-x3)*(x2-x1)) +
         y3*(x-x1)*(x-x2)/((x3-x1)*(x3-x2));
}

// Stock converter 0x100f0d90 and correction 0x100f0cc8. Brightness is a
// linear fraction. A nonzero channel occupies 8..100% electrical duty;
// zero is a separate off state, not an 8% brightness clamp.
inline Duties daylightFraction(unsigned kelvin, float brightness) {
  if (brightness > 1.0f) brightness = 1.0f;
  if (brightness <= 0.0f) return {0,0,0};
  if (kelvin < 2700) kelvin = 2700;
  if (kelvin >= 6400) kelvin = 6500;
  unsigned i = 0;
  while (i < 10 && kelvin >= calibration[i+1].kelvin) ++i;
  float warm=calibration[i].warm/100.0f, cold=calibration[i].cold/100.0f;
  // Stock holds the endpoint intervals constant. Interior intervals use
  // three-point interpolation, excluding the pure-cold endpoint.
  if (i != 0 && i < 9) {
    if (i == 8) --i;
    warm=interpolate(kelvin,calibration[i],calibration[i+1],calibration[i+2],true);
    cold=interpolate(kelvin,calibration[i],calibration[i+1],calibration[i+2],false);
    if (warm < 8.0f) { warm=0; cold=100; }
    if (cold < 8.0f) { cold=0; warm=100; }
  }
  const auto duty = [brightness](float coefficient) -> uint32_t {
    if (coefficient == 0) return 0;
    // Preserve the stock float operations, including its fused multiply-add.
    // Cancelling total/total changes rounding by one count on some inputs.
    const float uncorrected=(coefficient/100.0f*brightness)*kPeriodCounts;
    const float normalized=uncorrected/kPeriodCounts;
    const float corrected=__builtin_fmaf(-(0.08f-1.0f),normalized,0.08f);
    return uint32_t(corrected*kPeriodCounts+0.49f);
  };
  return {duty(warm),duty(cold),0};
}

inline Duties daylight(unsigned kelvin, unsigned brightness) {
  return daylightFraction(kelvin, (brightness > 1023 ? 1023 : brightness)/1023.0f);
}

inline uint32_t nightFraction(float brightness) {
  if (brightness > 1.0f) brightness = 1.0f;
  if (brightness <= 0.0f) return 0;
  return uint32_t(brightness*kPeriodCounts+0.49f);
}

// Input is the current faded/gamma-corrected Tasmota CCT frame, cold first.
// Derive temperature from this frame, not from the final requested CT.
inline Duties frame(unsigned cold, unsigned warm, bool night) {
  if (cold > 1023) cold = 1023;
  if (warm > 1023) warm = 1023;
  const unsigned total = cold+warm;
  if (!total) return {0,0,0};
  const unsigned brightness = total > 1023 ? 1023 : total;
  if (night) return {0,0,nightFraction(brightness/1023.0f)};
  if (!warm) return daylight(6500,brightness);
  if (!cold) return daylight(2700,brightness);
  const unsigned mired = kColdMired +
      ((kWarmMired-kColdMired)*warm + total/2)/total;
  return daylight(1000000u/mired,brightness);
}
}
#pragma GCC pop_options
