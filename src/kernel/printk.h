#pragma once

// Should be provided by drivers. By default, putch = uart_put_char.
extern void putch(char);

void printk(const char*, ...);
