// SPDX-License-Identifier: GPL-3.0-or-later
#include <Arduino.h>
#include <sys/time.h>
#include <errno.h>
extern "C" {
#include <FreeRTOS.h>
#include <task.h>
}

namespace {
int64_t wall_us = 0;
uint32_t wall_tick = 0;
// Caller holds the critical section. RtcSecond refreshes this at least once
// per main-loop second, so millis rollover is extended even without libc users.
void advance() {
  const uint32_t tick = millis();
  wall_us += int64_t(uint32_t(tick - wall_tick)) * 1000;
  wall_tick = tick;
}
}

extern "C" int settimeofday(const struct timeval* value, const struct timezone*) {
  if (value->tv_usec < 0 || value->tv_usec >= 1000000) {
    errno = EINVAL;
    return -1;
  }
  taskENTER_CRITICAL();
  wall_us = int64_t(value->tv_sec) * 1000000 + value->tv_usec;
  wall_tick = millis();
  taskEXIT_CRITICAL();
  return 0;
}

extern "C" int variant_gettimeofday(struct timeval* value, void*) {
  taskENTER_CRITICAL();
  advance();
  const int64_t snapshot = wall_us;
  taskEXIT_CRITICAL();
  value->tv_sec = snapshot / 1000000;
  value->tv_usec = snapshot % 1000000;
  if (value->tv_usec < 0) { --value->tv_sec; value->tv_usec += 1000000; }
  return 0;
}

extern "C" void mt7697_wall_clock_poll() {
  taskENTER_CRITICAL();
  advance();
  taskEXIT_CRITICAL();
}
