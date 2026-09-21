// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "ylxd01yl_light.h"

namespace ylxd01yl {
inline bool equal(Duties a,Duties b) {
  return a.warm==b.warm && a.cold==b.cold && a.night==b.night;
}

// Stock scalar 0x100f0964, with explicit operation order and monotonic clamp.
inline uint32_t scalar(uint32_t a,uint32_t b,unsigned k,unsigned n,
                       uint32_t previous,bool cubic) {
  if (k>=n) return b;
  double value;
  if (!cubic) value=(double(b)-a)*k/n+a;
  else {
    const float nf=n;
    const double denominator=(nf*nf)*nf;
    const double t=b>=a ? k:n-k;
    const double low=b>=a ? a:b, high=b>=a ? b:a;
    value=(((high-low)/denominator*t)*t)*t+low;
  }
  const uint32_t result=uint32_t(value);
  return b>=a ? (result<previous?previous:result) :
                (result>previous?previous:result);
}

inline Duties transition(Duties start,Duties end,unsigned k,unsigned n,
                         Duties previous,bool night,bool cubic) {
  if (!k) return start;
  if (k>=n) return end;
  if (night) return {0,0,scalar(start.night,end.night,k,n,previous.night,cubic)};
  uint32_t a[]={start.warm,start.cold}, b[]={end.warm,end.cold};
  for (unsigned i=0;i<2;++i) {
    if (!a[i] && b[i]) a[i]=320;
    else if (a[i] && !b[i]) b[i]=320;
  }
  if (!start.warm && !start.cold && end.warm && end.cold) {
    for (unsigned i=0;i<2;++i) a[i]=scalar(a[i],b[i],n*3/7,n,320,cubic);
  } else if (start.warm && start.cold && !end.warm && !end.cold) {
    for (unsigned i=0;i<2;++i) b[i]=scalar(a[i],b[i],n*4/7,n,320,cubic);
  }
  return {scalar(a[0],b[0],k,n,previous.warm,cubic),
          scalar(a[1],b[1],k,n,previous.cold,cubic),0};
}

// Wall-clock scheduler for ordinary setting replacement. Each request starts
// at the last emitted duty, never at a superseded command's target.
class Fade {
 public:
  void set(Duties end,bool night,unsigned brightness,uint32_t duration,uint32_t now) {
    if (night_!=night) { current_={0,0,0}; completed_brightness_=0; }
    night_=night;
    start_=current_; target_=end; requested_brightness_=brightness;
    cubic_=night && completed_brightness_!=brightness;
    began_=now; step_=0;
    if (!duration || equal(start_,target_)) {
      current_=target_; steps_=0; completed_brightness_=brightness;
      return;
    }
    if (duration<300) duration=300;
    if (duration>3600000) duration=3600000;
    steps_=(duration+9)/10;
  }
  bool poll(uint32_t now) {
    if (!active()) return false;
    unsigned due=(now-began_)/10+1; // Stock queues its first tick immediately.
    if (due>steps_) due=steps_;
    if (due<=step_) return false;
    step_=due;
    current_=transition(start_,target_,step_,steps_,current_,night_,cubic_);
    if (!active()) completed_brightness_=requested_brightness_;
    return true;
  }
  bool active() const { return step_<steps_; }
  Duties current() const { return current_; }
  Duties target() const { return target_; }
  void off() { current_=start_=target_={0,0,0}; steps_=step_=0; completed_brightness_=0; }
 private:
  Duties current_{},start_{},target_{};
  uint32_t began_=0;
  unsigned step_=0,steps_=0,requested_brightness_=0,completed_brightness_=0;
  bool night_=false,cubic_=false;
};
}
