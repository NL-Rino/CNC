#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
typedef void* QueueHandle_t; typedef void* TaskHandle_t; typedef int BaseType_t;
#define pdTRUE 1
#define pdMS_TO_TICKS(x) (x)
#define portMAX_DELAY 0xffffffff
#define IRAM_ATTR
