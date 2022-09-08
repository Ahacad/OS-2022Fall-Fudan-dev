#include <common/spinlock.h>
#include <common/string.h>
#include <kernel/mem.h>
#include <kernel/init.h>
#include <kernel/printk.h>
#include <kernel/sched.h>

NO_BSS static bool boot_secondary_cpus;
static char hello[16];

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

static void test(u64 id)
{
    while (1)
    {
        printk("proc %d at cpu %d\n", id, cpuid());
        arch_with_trap
        {
            delay_us(1000*4000);
        }
        sched();
    }
}

NO_RETURN void kernel_entry()
{
    do_rest_init();

    printk("hello world\n");

    for (int i = 1; i < 10; i++)
    {
        struct proc* p = kalloc(sizeof(struct proc));
        init_proc(p);
        start_proc(p, &test, i);
    }

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
