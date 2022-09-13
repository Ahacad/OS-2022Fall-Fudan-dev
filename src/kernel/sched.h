#pragma once

#include <kernel/proc.h>

#define RR_TIME 1000

void init_schinfo(struct schinfo*);

void sched(enum procstate new_state);
void activate_sched(struct proc*);
void deactivate_sched(struct proc*);
#define yield() sched(RUNNABLE)

struct proc* thisproc();

NO_INLINE NO_RETURN void _panic(const char*, int);
#define PANIC() _panic(__FILE__, __LINE__)
#define ASSERT(expr) ({ if (!(expr)) PANIC(); })
