#pragma once
#include <stdint.h>
#define HAL_TRNG_STATUS_OK 0
int hal_trng_init(void);
int hal_trng_get_generated_random_number(uint32_t*);
int hal_trng_deinit(void);
