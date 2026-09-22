#pragma once
#include <stdint.h>
typedef enum { HAL_RTC_STATUS_OK = 0, HAL_RTC_STATUS_ERROR = -2 } hal_rtc_status_t;
hal_rtc_status_t hal_rtc_get_data(uint16_t, char*, uint16_t);
hal_rtc_status_t hal_rtc_set_data(uint16_t, const char*, uint16_t);
