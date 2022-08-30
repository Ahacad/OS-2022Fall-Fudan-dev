#pragma once

#include <common/defines.h>

#define early_init_func(func) __attribute__((section(".init.early"))) static void* __init_##func = &func;
#define init_func(func) __attribute__((section(".init"))) static void* __init_##func = &func;

void do_early_init();
void do_init();
