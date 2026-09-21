// SPDX-License-Identifier: GPL-3.0-or-later
#include <stdint.h>
extern "C" {
#include <FreeRTOS.h>
#include <task.h>
#include <variant_delay.h>
}

extern "C" uint32_t variant_millis() {
  return uint64_t(xTaskGetTickCount()) * 1000 / configTICK_RATE_HZ;
}

extern "C" void variant_delay(uint32_t ms) {
  // Round positive delays up so a sub-tick sleep still gives other tasks time.
  vTaskDelay((uint64_t(ms) * configTICK_RATE_HZ + 999) / 1000);
}
