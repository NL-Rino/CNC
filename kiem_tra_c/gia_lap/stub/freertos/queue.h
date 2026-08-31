#pragma once
#include "freertos/FreeRTOS.h"
QueueHandle_t xQueueCreate(int,int);
int xQueueReceive(QueueHandle_t,void*,unsigned);
int xQueueSend(QueueHandle_t,const void*,unsigned);
void xQueueReset(QueueHandle_t);
int xQueueSendToFront(QueueHandle_t,const void*,unsigned);
