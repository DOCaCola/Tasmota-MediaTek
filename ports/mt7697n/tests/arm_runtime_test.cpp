#include "../ylxd01yl_fade.h"
#include "stock_fade_vectors.h"
// SPDX-License-Identifier: GPL-3.0-or-later
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/time.h>
#include "../ylxd01yl_light.h"
#include "stock_light_vectors.h"
#include "../../../lib/default/Ext-printf/src/ext_printf.h"
#include "../../../lib/default/Ext-printf/src/arm_va.h"

static char heap[65536];
extern "C" void* _sbrk(int bytes) {
  static unsigned used;
  if (bytes < 0 || unsigned(bytes) > sizeof(heap) - used) { return (void*)-1; }
  void* result = heap + used;
  used += bytes;
  return result;
}
static uint32_t tick;
extern "C" uint32_t millis() { return tick; }
extern "C" void vPortEnterCritical() {}
extern "C" void vPortExitCritical() {}
extern "C" int variant_gettimeofday(struct timeval*, void*);
extern "C" void mt7697_wall_clock_poll();

__attribute__((noinline)) static bool arguments(unsigned count, ...) {
  va_list args, copy;
  va_start(args, count);
  va_copy(copy, args);
  for (unsigned i = 0; i < count; ++i) {
    if (va_arg(args, uint32_t) != i + 1) { return false; }
    *static_cast<uint32_t*>(ext_arm_previous_word(args)) = 100 + i;
  }
  for (unsigned i = 0; i < count; ++i) {
    if (va_arg(copy, uint32_t) != 100 + i) { return false; }
  }
  va_end(copy); va_end(args);
  return true;
}

extern "C" int run_tests() {
  for (const auto& p : stock_vectors) {
    const auto d=ylxd01yl::daylightFraction(p.kelvin,p.percent/100.0f);
    if (d.warm!=p.warm || d.cold!=p.cold) return 13;
  }
  for (unsigned b=0;b<=100;++b) {
    if (ylxd01yl::nightFraction(b/100.0f)!=b*40) return 14;
  }
  // Check register-save and stack slots, including replay of rewritten args.
  if (!arguments(12, 1,2,3,4,5,6,7,8,9,10,11,12)) { return 1; }
  char buffer[256];
  ext_snprintf_P(buffer, sizeof(buffer), "%s %u %d %08X %_I",
                 "lamp", 123U, -4, 0x12abU, 0x04030201U);
  if (strcmp(buffer, "lamp 123 -4 000012AB 1.2.3.4")) { return 2; }
  float number = 3.125f;
  uint64_t large = 18446744073709551615ULL;
  uint8_t bytes[] = {0, 0xab, 0xff};
  ext_snprintf_P(buffer, sizeof(buffer), "%*_f %_U %_X %3_H %8_b %s",
                 3, &number, &large, &large, bytes, 5U, "end");
  if (strcmp(buffer, "3.125 18446744073709551615 FFFFFFFFFFFFFFFF 00ABFF 00000101 end")) {
    return 3;
  }
  char* dynamic = ext_snprintf_malloc_P("%u %_I %*_f %s", 7U, 0x04030201U, -3, &number, "ok");
  if (!dynamic || strcmp(dynamic, "7 1.2.3.4 3.125 ok")) { return 4; }
  free(dynamic);
  int length = ext_snprintf_P(buffer, 5, "%s", "long string");
  if (length != 11 || strcmp(buffer, "long")) { return 5; }
  ext_snprintf_P(buffer, sizeof(buffer), "%.*f %_I %lu %_U", 2, 1.25,
                 0x04030201U, 42UL, &large);
  if (strcmp(buffer, "1.25 1.2.3.4 42 18446744073709551615")) { return 10; }
  // Check actual SDK-runtime wall-clock representation and millis rollover.
  timeval set = {1700000000, 750000}, got;
  tick = 0xffffff00;
  if (settimeofday(&set, nullptr)) { return 6; }
  tick += 1250;
  if (variant_gettimeofday(&got, nullptr) || got.tv_sec != 1700000002 || got.tv_usec) {
    return 7;
  }
  set.tv_usec = 1000000;
  if (settimeofday(&set, nullptr) != -1) { return 8; }
  tick += 500;
  mt7697_wall_clock_poll();
  if (variant_gettimeofday(&got, nullptr) || got.tv_sec != 1700000002 || got.tv_usec != 500000) {
    return 9;
  }
  set.tv_sec = -1; set.tv_usec = 750000;
  if (settimeofday(&set, nullptr)) { return 11; }
  tick += 125;
  if (variant_gettimeofday(&got, nullptr) || got.tv_sec != -1 || got.tv_usec != 875000) {
    return 12;
  }
  for (const auto& v:stock_fades) {
    if (!ylxd01yl::equal(ylxd01yl::transition(v.start,v.end,v.k,v.n,v.previous,v.night,v.cubic),v.result)) return 20;
  }
  for (const auto& v:stock_targets) {
    if (!ylxd01yl::equal(ylxd01yl::target(v.kelvin,v.percent,false),v.result)) return 21;
  }
  return 0;
}
