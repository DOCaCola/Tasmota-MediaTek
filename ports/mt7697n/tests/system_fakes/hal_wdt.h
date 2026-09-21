#pragma once
typedef enum {
  HAL_WDT_TIMEOUT_RESET = 0, HAL_WDT_SOFTWARE_RESET = 1, HAL_WDT_NONE_RESET = 2
} hal_wdt_reset_status_t;
hal_wdt_reset_status_t hal_wdt_get_reset_status(void);
