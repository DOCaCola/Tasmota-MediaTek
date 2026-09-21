// SPDX-License-Identifier: GPL-3.0-or-later
#include "../ylxd01yl_light.h"
#include <cassert>
#include <cstdio>
#include "stock_light_vectors.h"
int main() {
  using namespace ylxd01yl;
  assert(frame(0,0,false).warm == 0);
  assert(frame(1023,0,false).cold == 4000);
  assert(frame(0,1023,false).warm == 4000);
  assert(frame(1023,1023,false).warm + frame(1023,1023,false).cold <= 4000);
  for (const auto& p : stock_vectors) {
    const auto d = daylightFraction(p.kelvin,p.percent/100.0f);
    assert(d.warm == p.warm && d.cold == p.cold);
  }
  // Sweep the complete physical temperature and brightness domain.
  for (unsigned k=2700; k<=6500; ++k) {
    Duties prev = {};
    for (unsigned b=0; b<=1023; ++b) {
      const auto d=daylight(k,b);
      assert(d.warm+d.cold<=4000 && !d.night);
      assert(!d.warm || d.warm>=320);
      assert(!d.cold || d.cold>=320);
      assert(!b || d.warm || d.cold);
      assert(d.warm>=prev.warm && d.cold>=prev.cold);
      prev=d;
    }
  }
  for (unsigned c=0;c<=1023;++c) {
    for (unsigned w=0;w<=1023;++w) {
      const auto day=frame(c,w,false), night=frame(c,w,true);
      assert(day.warm+day.cold<=4000 && !day.night);
      assert(!night.warm && !night.cold && night.night<=4000);
    }
  }
  assert(daylight(0,1023).warm==4000);
  assert(daylight(10000,1023).cold==4000);
  for (unsigned b=0;b<=100;++b) assert(nightFraction(b/100.0f)==b*40);
  puts("Lamp calibration anchors, full-range bounds, brightness monotonicity and mode isolation passed");
}
