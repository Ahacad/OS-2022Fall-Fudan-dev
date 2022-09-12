#pragma once

#include <kernel/proc.h>
#include <common/list.h>

#define RR_TIME 1000

struct sched
{
    struct proc* proc;
    struct proc* idle;
};

struct schinfo
{
    ListNode runqueue;
};

void init_schinfo(struct schinfo*);

void sched();

struct proc* thisproc();

NO_INLINE NO_RETURN void _panic(const char*, int);
#define PANIC() _panic(__FILE__, __LINE__)
#define ASSERT(expr) ({ if (!(expr)) PANIC(); })
