#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include "../ylxd01yl_pwm.h"
extern "C" {
#include "hal_gpio.h"
#include "hal_pinmux.h"
#include "hal_pwm.h"
}

static uint32_t duties[34];
static bool initialized[34], muxed[34], stopped[34], fail_next;
static unsigned writes;

extern "C" {
hal_pwm_status_t hal_pwm_init(hal_pwm_source_clock_t clock) {
  assert(clock == HAL_PWM_CLOCK_40MHZ);
  return HAL_PWM_STATUS_OK;
}
hal_pwm_status_t hal_pwm_set_frequency(hal_pwm_channel_t channel, uint32_t hz, uint32_t* total) {
  assert(channel >= 31 && channel <= 33);
  assert(hz == 10000);
  *total = 4000;
  return HAL_PWM_STATUS_OK;
}
hal_pwm_status_t hal_pwm_set_duty_cycle(hal_pwm_channel_t channel, uint32_t duty) {
  if (fail_next) { fail_next = false; return HAL_PWM_STATUS_ERROR; }
  duties[channel] = duty;
  initialized[channel] = true;
  ++writes;
  // These invariants are checked at every hardware write, not just at the end.
  assert(duties[32] + duties[33] <= 4000);
  assert(duties[31] <= 4000);
  assert(duties[31] == 0 || (duties[32] == 0 && duties[33] == 0));
  return HAL_PWM_STATUS_OK;
}
hal_pinmux_status_t hal_pinmux_set_function(hal_gpio_pin_t pin, uint8_t mux) {
  assert(pin >= 30 && pin <= 32 && mux == 9);
  assert(initialized[31] && initialized[32] && initialized[33]);
  assert(duties[31] == 0 && duties[32] == 0 && duties[33] == 0);
  muxed[pin + 1] = true;
  return HAL_PINMUX_STATUS_OK;
}
hal_pwm_status_t hal_pwm_start(hal_pwm_channel_t channel) {
  assert(muxed[channel] && duties[channel] == 0);
  return HAL_PWM_STATUS_OK;
}
hal_pwm_status_t hal_pwm_stop(hal_pwm_channel_t channel) {
  stopped[channel] = true;
  return HAL_PWM_STATUS_OK;
}
}

int main() {
  ylxd01yl::Pwm light;
  assert(!light.daylight(1, 0));
  assert(light.begin());
  assert(light.daylight(0, 4000));
  assert(duties[33] == 4000);
  assert(light.daylight(4000, 0));
  assert(duties[32] == 4000);
  assert(light.night(4000));
  assert(duties[31] == 4000);
  assert(light.daylight(1000, 2000));
  assert(light.night(1));
  assert(light.off());
  const unsigned before = writes;
  assert(!light.daylight(4000, 1));
  assert(!light.daylight(UINT_MAX, 1));
  assert(!light.night(4001));
  assert(writes == before);
  assert(light.daylight(500, 500));
  fail_next = true;
  assert(!light.night(100));
  assert(duties[31] == 0 && duties[32] == 0 && duties[33] == 0);
  assert(stopped[31] && stopped[32] && stopped[33]);
  assert(!light.daylight(1, 0));
  puts("PWM invariants passed: zero-before-mux, physical channel mapping, transition envelope, exclusion, bounds, HAL failure.");
}
