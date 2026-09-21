// SPDX-License-Identifier: GPL-3.0-or-later
#include "../ylxd01yl_fade.h"
#include "stock_fade_vectors.h"
#include <cassert>
#include <cstdio>
int main() {
  using namespace ylxd01yl;
  for (const auto& v:stock_fades) {
    const auto d=transition(v.start,v.end,v.k,v.n,v.previous,v.night,v.cubic);
    assert(equal(d,v.result));
    assert(d.warm<=4000 && d.cold<=4000 && d.warm+d.cold<=4320);
    assert(!d.night || (!d.warm && !d.cold));
  }
  for (const auto& v:stock_targets) assert(equal(target(v.kelvin,v.percent,false),v.result));
  Fade f;
  f.set({4000,0,0},false,100,0,0);
  f.set({0,4000,0},false,100,500,0xfffffff0u);
  f.poll(0xfffffff0u);
  assert(equal(f.current(),transition({4000,0,0},{0,4000,0},1,50,{4000,0,0},false,false)));
  assert(!f.poll(0xfffffff0u));
  f.poll(84); // millis wrap, 100 ms elapsed.
  const auto actual=f.current();
  const auto next=target(4000,1,false);
  f.set(next,false,1,500,84);f.poll(84);
  assert(equal(f.current(),transition(actual,next,1,50,actual,false,false)));
  f.poll(584);assert(equal(f.current(),next) && !f.active());
  f.set({0,0,200},true,5,1,600);
  f.poll(600);assert(!f.current().warm && !f.current().cold);
  f.poll(890);assert(f.current().night==200 && !f.active());
  f.set({0,0,4000},true,100,3600001,900);
  f.poll(3599990+900);assert(!f.active() && f.current().night==4000);
  f.off();assert(equal(f.current(),{0,0,0}) && !f.active());
  puts("Stock ARM fade vectors, runtime targets, replacement, timer wrap and group exclusion passed");
}
