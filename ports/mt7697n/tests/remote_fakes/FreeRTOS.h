#pragma once
#include <stddef.h>
#include <stdint.h>
typedef uint32_t StackType_t;
typedef void* TaskHandle_t;
typedef void* QueueHandle_t;
#define pdPASS 1
#define pdTRUE 1
QueueHandle_t xQueueCreate(unsigned,unsigned);
int xQueueSend(QueueHandle_t,const void*,unsigned);
int xQueueReceive(QueueHandle_t,void*,unsigned);
void vQueueDelete(QueueHandle_t);
int xTaskCreate(void (*)(void*),const char*,unsigned,void*,unsigned,TaskHandle_t*);
