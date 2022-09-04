#include <kernel/sched.h>
#include <kernel/init.h>
#include <kernel/mem.h>
#include <kernel/printk.h>

struct cpu cpus[NCPU];

extern SpinLock proc_list_lock;
extern struct proc root_proc;

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

static bool simple_sched()
{
    setup_checker(0);
    struct proc* this = cpus[cpuid()].proc, * next = NULL;
    acquire_spinlock(0, &proc_list_lock);
    _for_in_list(p, this->state == IDLE ? &root_proc.list : &this->list)
    {
        struct proc* proc = container_of(p, struct proc, list);
        if (proc->state == RUNNABLE)
        {
            next = proc;
            break;
        }
    }
    switch (this->state)
    {
        case RUNNING:
            if (next)
            {
                this->state = RUNNABLE;
                next->state = RUNNING;
            }
            break;
        case IDLE:
            if (next)
                next->state = RUNNING;
            break;
    }
    if (next)
        swtch(next->kcontext, &this->kcontext);
    release_spinlock(0, &proc_list_lock);
}

__attribute__((weak, alias("simple_sched"))) void sched();

NO_RETURN void idle_entry()
{
    printk("cpu%d hello\n", cpuid());

    while (1)
    {
        simple_sched();
        // arch_wfi();
    }
}