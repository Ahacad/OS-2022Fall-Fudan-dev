#include <common/spinlock.h>
#include <kernel/init.h>
#include <kernel/printk.h>

void kernel_init()
{
    do_early_init();
    do_init();
    printk("hello world\n");
}

NO_RETURN void main()
{
    if (cpuid() == 0)
        kernel_init();

    while (1);
}