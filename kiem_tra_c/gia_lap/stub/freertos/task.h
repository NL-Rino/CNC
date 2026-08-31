#pragma once
void vTaskDelay(unsigned);
int xTaskCreatePinnedToCore(void(*)(void*),const char*,int,void*,int,void**,int);
unsigned xTaskGetTickCount(void);
#define portTICK_PERIOD_MS 1
