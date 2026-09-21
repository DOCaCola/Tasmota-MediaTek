// SPDX-License-Identifier: GPL-3.0-or-later
#include "ylxd01yl_pwm.h"
extern "C" {
#include <hal_gpio.h>
#include <hal_pinmux.h>
#include <hal_pwm.h>
}

namespace ylxd01yl {
namespace {
const hal_gpio_pin_t pins[] = {HAL_GPIO_31, HAL_GPIO_32, HAL_GPIO_30};
const hal_pwm_channel_t channels[] = {HAL_PWM_32, HAL_PWM_33, HAL_PWM_31};
}

bool Pwm::begin() {
  ready_ = false;
  if (hal_pwm_init(HAL_PWM_CLOCK_40MHZ) != HAL_PWM_STATUS_OK) return false;
  // Set zero duty on every channel before routing any of them to the pins.
  for (unsigned i = 0; i < 3; ++i) {
    uint32_t total = 0;
    if (hal_pwm_set_frequency(channels[i], kFrequencyHz, &total) != HAL_PWM_STATUS_OK ||
        total != kPeriodCounts ||
        hal_pwm_set_duty_cycle(channels[i], 0) != HAL_PWM_STATUS_OK) return false;
    duties_[i] = 0;
  }
  for (unsigned i = 0; i < 3; ++i) {
    if (hal_pinmux_set_function(pins[i], 9) != HAL_PINMUX_STATUS_OK ||
        hal_pwm_start(channels[i]) != HAL_PWM_STATUS_OK) return false;
  }
  ready_ = true;
  return true;
}

bool Pwm::apply(uint32_t warm, uint32_t cold, uint32_t night) {
  if (!ready_) return false;
  const uint32_t next[] = {warm, cold, night};
  // Reduce outputs before increasing others. This prevents an intermediate
  // sum above the requested envelope and overlap during day/night handover.
  for (unsigned phase = 0; phase < 2; ++phase) {
    for (unsigned i = 0; i < 3; ++i) {
      if ((phase == 0 && next[i] < duties_[i]) ||
          (phase == 1 && next[i] > duties_[i])) {
        if (hal_pwm_set_duty_cycle(channels[i], next[i]) != HAL_PWM_STATUS_OK) {
          ready_ = false;
          for (unsigned j = 0; j < 3; ++j) {
            hal_pwm_set_duty_cycle(channels[j], 0);
            hal_pwm_stop(channels[j]);
          }
          return false;
        }
        duties_[i] = next[i];
      }
    }
  }
  return true;
}

bool Pwm::daylight(uint32_t warm, uint32_t cold) {
  // Conservative combined duty envelope; no independent full-power channels.
  if (warm > kPeriodCounts || cold > kPeriodCounts - warm) return false;
  return apply(warm, cold, 0);
}

bool Pwm::night(uint32_t duty) {
  if (duty > kPeriodCounts) return false;
  return apply(0, 0, duty);
}

bool Pwm::off() { return apply(0, 0, 0); }
}  // namespace ylxd01yl
