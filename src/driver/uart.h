#pragma once

char uart_get_char();
void uart_put_char(char c);

#define uart_valid_char(c) (c != 0xff)