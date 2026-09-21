#pragma once
#include <stdint.h>
#define ERR_OK 0
typedef struct { uint32_t addr; } ip_addr_t;
int netconn_gethostbyname(const char*, ip_addr_t*);
