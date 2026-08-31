#pragma once
#include <stdint.h>
#define UART_NUM_0 0
#define UART_DATA_8_BITS 3
#define UART_PARITY_DISABLE 0
#define UART_STOP_BITS_1 1
#define UART_HW_FLOWCTRL_DISABLE 0
#define UART_SCLK_DEFAULT 0
typedef struct { int baud_rate, data_bits, parity, stop_bits, flow_ctrl, source_clk; } uart_config_t;
int uart_param_config(int,const uart_config_t*);
int uart_driver_install(int,int,int,int,void*,int);
int uart_read_bytes(int,void*,int,unsigned);
int uart_set_baudrate(int, unsigned);
int uart_wait_tx_done(int, unsigned);
int uart_flush_input(int);
