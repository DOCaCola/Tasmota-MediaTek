#pragma once
#include "hal_gpio.h"
typedef enum {HAL_PINMUX_STATUS_OK = 0} hal_pinmux_status_t;
hal_pinmux_status_t hal_pinmux_set_function(hal_gpio_pin_t, uint8_t);
