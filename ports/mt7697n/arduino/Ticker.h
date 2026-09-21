// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <stdlib.h>
extern "C" {
#include <FreeRTOS.h>
#include <timers.h>
}

// Static-lifetime application timers. Callbacks run in the FreeRTOS timer task;
// they must not block. Matches the existing Tasmota RTC/input timer context.
class Ticker {
 public:
  Ticker() = default;
  Ticker(const Ticker&) = delete;
  Ticker& operator=(const Ticker&) = delete;
  void attach(float seconds, void (*callback)()) {
    attach_ms(static_cast<unsigned long>(seconds * 1000), callback);
  }
  void attach_ms(unsigned long milliseconds, void (*callback)()) {
    if (!milliseconds || !callback) { abort(); }
    callback_ = callback;
    TickType_t ticks = milliseconds / portTICK_PERIOD_MS +
                       (milliseconds % portTICK_PERIOD_MS != 0);
    if (!timer_) {
      timer_ = xTimerCreate("Tasmota", ticks, pdTRUE, this, dispatch);
      if (!timer_) { abort(); }
    }
    const BaseType_t changed = xTimerChangePeriod(timer_, ticks, portMAX_DELAY);
    if (changed != pdPASS) { abort(); }
  }
  void detach() {
    if (timer_) {
      const BaseType_t stopped = xTimerStop(timer_, portMAX_DELAY);
      if (stopped != pdPASS) { abort(); }
    }
  }
 private:
  static void dispatch(TimerHandle_t timer) {
    static_cast<Ticker*>(pvTimerGetTimerID(timer))->callback_();
  }
  TimerHandle_t timer_ = nullptr;
  void (*callback_)() = nullptr;
};
