#pragma once

#include <common/list.h>
struct proc; // dont include proc.h here

// embedded data for cpus
struct sched
{
    struct proc* proc;
    struct proc* idle;
};

// embeded data for procs
struct schinfo
{
    bool group;
    ListNode sqnode;
};

// embedded data for containers
struct schqueue
{
    ListNode sqhead;
};
