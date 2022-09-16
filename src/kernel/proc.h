#pragma once

#include <common/defines.h>
#include <common/list.h>
#include <kernel/schinfo.h>

enum procstate { UNUSED, RUNNABLE, RUNNING, SLEEPING };

typedef struct UserContext
{
    u64 ttbr0;
    u64 tpidr;
    u64 usp;
    u64 elr;
    u64 spsr;
    u64 lr;
    u64 x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17;
} UserContext;

typedef struct KernelContext
{
    u64 lr;
    u64 x0, x1;
    u64 x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29;
} KernelContext;

struct proc
{
    bool killed;
    bool idle;
    int pid;
    enum procstate state; // bind to list
    ListNode children;
    ListNode ptnode;
    struct proc* parent;
    struct schinfo schinfo;
    void* kstack;
    ListNode slnode;
    UserContext* ucontext;
    KernelContext* kcontext; // also sp_el1
};

void init_proc(struct proc*);
void start_proc(struct proc*, void(*entry)(u64), u64 arg);
// set proc->parent to parent without updating the children list of the old parent
// void reset_proc_parent(struct proc* proc, struct proc* parent);
