#pragma once
#include <stdint.h>
#define NVDM_STATUS_OK 0
#define NVDM_STATUS_ITEM_NOT_FOUND 1
#define NVDM_DATA_ITEM_TYPE_RAW_DATA 0
int nvdm_read_data_item(const char*,const char*,uint8_t*,uint32_t*);
int nvdm_write_data_item(const char*,const char*,int,const uint8_t*,uint32_t);
