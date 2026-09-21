#pragma once
#include <stdint.h>
typedef enum {HAL_PWM_31 = 31, HAL_PWM_32, HAL_PWM_33} hal_pwm_channel_t;
typedef enum {HAL_PWM_CLOCK_40MHZ = 4} hal_pwm_source_clock_t;
typedef enum {HAL_PWM_STATUS_OK = 0, HAL_PWM_STATUS_ERROR = -4} hal_pwm_status_t;
typedef enum {HAL_PWM_IDLE=0, HAL_PWM_BUSY=1} hal_pwm_running_status_t;
hal_pwm_status_t hal_pwm_init(hal_pwm_source_clock_t);
hal_pwm_status_t hal_pwm_set_frequency(hal_pwm_channel_t, uint32_t, uint32_t*);
hal_pwm_status_t hal_pwm_set_duty_cycle(hal_pwm_channel_t, uint32_t);
hal_pwm_status_t hal_pwm_start(hal_pwm_channel_t);
hal_pwm_status_t hal_pwm_stop(hal_pwm_channel_t);
hal_pwm_status_t hal_pwm_get_running_status(hal_pwm_channel_t, hal_pwm_running_status_t*);
hal_pwm_status_t hal_pwm_get_frequency(hal_pwm_channel_t, uint32_t*);
hal_pwm_status_t hal_pwm_get_duty_cycle(hal_pwm_channel_t, uint32_t*);
