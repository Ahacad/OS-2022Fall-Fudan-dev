#include <common/spinlock.h>
#include <common/string.h>
#include <kernel/mem.h>
#include <kernel/init.h>
#include <kernel/printk.h>
#include <kernel/sched.h>

NO_BSS static bool boot_secondary_cpus;

NO_RETURN void idle_entry();

NO_RETURN void kernel_init()
{
    // clear BSS section.
    extern char edata[], end[];
    memset(edata, 0, (usize)(end - edata));

    do_early_init();
    do_init();
    boot_secondary_cpus = true;
    idle_entry();
}

extern PTEntries kernel_pt;

NO_RETURN void kernel_entry()
{
    printk("hello world\n");

    do_rest_init();

    printk("%llx %llx\n", arch_get_ttbr0(), &kernel_pt);

    while (1)
        sched();
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
