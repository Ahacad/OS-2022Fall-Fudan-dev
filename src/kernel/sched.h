#pragma once

#include <kernel/proc.h>

#define NCPU 4

struct cpu
{
    struct proc* proc;
    struct proc* idle;
};

extern struct cpu cpus[NCPU];

