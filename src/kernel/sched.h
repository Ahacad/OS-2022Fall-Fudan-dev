#pragma once

#include <kernel/proc.h>

#define RR_TIME 1000

struct sched
{
    struct proc* proc;
    struct proc* idle;
};

extern void swtch(KernelContext* new_ctx, KernelContext** old_ctx);

void sched();

struct proc* thisproc();

NO_INLINE NO_RETURN void _panic(const char*, int);
#define PANIC() _panic(__FILE__, __LINE__)
#define ASSERT(expr) ({ if (!(expr)) PANIC(); })
