#include <common/spinlock.h>
#include <kernel/mem.h>
#include <kernel/init.h>
#include <kernel/printk.h>

static bool boot_secondary_cpus;
NO_RETURN void idle_entry();

NO_RETURN void kernel_init()
{
    do_early_init();

    do_init();

    boot_secondary_cpus = true;

    idle_entry();
}

NO_RETURN void kernel_entry()
{
    do_rest_init();

    printk("hello world\n");

    while (1);
}

NO_RETURN void main()
{
    if (cpuid() == 0)
    {
        kernel_init();
    }

    while (!boot_secondary_cpus);
    arch_dsb_sy();

    idle_entry();
}
