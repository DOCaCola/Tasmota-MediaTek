#pragma once
#include <stdint.h>
#define HAL_SYS_REBOOT_MAGIC 0x1
int hal_sys_reboot(uint32_t, uint32_t);
