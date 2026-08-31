#pragma once
#include <stdint.h>
typedef int gpio_num_t; typedef int esp_err_t;
#define GPIO_NUM_0 0
#define GPIO_NUM_1 1
#define GPIO_NUM_2 2
#define GPIO_NUM_3 3
#define GPIO_NUM_5 5
#define GPIO_NUM_6 6
#define GPIO_NUM_7 7
#define GPIO_NUM_8 8
#define GPIO_NUM_9 9
#define GPIO_NUM_10 10
#define GPIO_NUM_11 11
#define GPIO_NUM_12 12
#define GPIO_NUM_15 15
#define GPIO_NUM_20 20
#define GPIO_NUM_24 24
#define GPIO_NUM_28 28
#define GPIO_NUM_29 29
#define GPIO_NUM_30 30
#define GPIO_NUM_31 31
#define GPIO_NUM_34 34
#define GPIO_NUM_35 35
#define GPIO_NUM_36 36
#define GPIO_NUM_37 37
#define GPIO_NUM_38 38
#define GPIO_NUM_39 39

#define GPIO_NUM_4 4
#define GPIO_NUM_13 13
#define GPIO_NUM_14 14
#define GPIO_NUM_16 16
#define GPIO_NUM_25 25
#define GPIO_NUM_26 26
#define GPIO_NUM_19 19
#define GPIO_NUM_23 23
#define GPIO_NUM_27 27
#define GPIO_NUM_32 32
#define GPIO_NUM_33 33
#define GPIO_NUM_17 17
#define GPIO_NUM_18 18
#define GPIO_NUM_21 21
#define GPIO_NUM_22 22
#define GPIO_MODE_INPUT 1
#define GPIO_MODE_OUTPUT 2
#define GPIO_PULLUP_ENABLE 1
#define GPIO_PULLDOWN_DISABLE 0
#define GPIO_INTR_NEGEDGE 2
#define GPIO_INTR_ANYEDGE 3
#define GPIO_INTR_DISABLE 0
typedef struct { uint64_t pin_bit_mask; int mode, pull_up_en, pull_down_en, intr_type; } gpio_config_t;
esp_err_t gpio_config(const gpio_config_t*);
void gpio_set_level(int,int);
int gpio_get_level(int);
esp_err_t gpio_install_isr_service(int);
esp_err_t gpio_isr_handler_add(int,void(*)(void*),void*);
