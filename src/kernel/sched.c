#include <kernel/sched.h>
#include <kernel/init.h>
#include <kernel/mem.h>
#include <kernel/printk.h>

struct cpu cpus[NCPU];

static void init_sched()
{
    for (int i = 0; i < 4; i++)
    {
        struct proc* p = kalloc(sizeof(struct proc));
        p->pid = 0;
        p->state = IDLE;
        cpus[i].proc = cpus[i].idle = p;
    }
}
init_func(init_sched);

NO_RETURN void idle_entry()
{
    printk("cpu%d hello\n", cpuid());

    while (1);
}