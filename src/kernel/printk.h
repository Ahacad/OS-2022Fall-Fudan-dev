#pragma once

#include <driver/uart.h>

#define putchar uart_put_char
void printk(const char *fmt, ...);

