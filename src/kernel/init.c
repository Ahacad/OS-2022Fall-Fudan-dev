#include <kernel/init.h>

void early_init();
void init();
void einit();

void do_early_init()
{
    for (u64 p = (u64)early_init; p < (u64)init; p += sizeof(void*))
        ((void(*)())p)();
}

void do_init()
{
    for (u64 p = (u64)init; p < (u64)einit; p += sizeof(void*))
        ((void(*)())p)();
}