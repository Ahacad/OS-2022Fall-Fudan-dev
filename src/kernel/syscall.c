#include <kernel/syscall.h>

void* syscall_table[NR_SYSCALL];

void syscall_entry(UserContext* context)
{
    u64 callno = context->x8;
    u64 ret = -1ull;
    if (callno < NR_SYSCALL && syscall_table[callno])
        ret = ((u64(*)(u64, u64, u64, u64, u64, u64))syscall_table[callno])(context->x0, context->x1, context->x2, context->x3, context->x4, context->x5);
    context->x0 = ret;
}

