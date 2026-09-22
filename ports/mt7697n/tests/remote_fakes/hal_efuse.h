#pragma once
#include <stdint.h>
#define HAL_EFUSE_OK 0
int hal_efuse_read(uint32_t,uint8_t*,uint32_t);
