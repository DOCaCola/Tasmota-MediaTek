#pragma once
#include <stddef.h>
#include <stdint.h>
typedef uint32_t StackType_t;
size_t xPortGetFreeHeapSize(void);
size_t xPortGetMinimumEverFreeHeapSize(void);
