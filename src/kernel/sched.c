#include <kernel/sched.h>
#include <kernel/init.h>
#include <kernel/mem.h>
#include <kernel/printk.h>
#include <aarch64/intrinsic.h>
#include <kernel/cpu.h>
#include <driver/clock.h>

static bool panic_flag;

extern SpinLock proc_list_lock;
extern struct proc root_proc;

struct proc* thisproc()
{
    return cpus[cpuid()].sched->proc;
}

define_init(sched)
{
    for (int i = 0; i < NCPU; i++)
    {
        struct proc* p = kalloc(sizeof(struct proc));
        p->pid = 0;
        p->state = IDLE;
        struct sched* sh = kalloc(sizeof(struct sched));
        sh->proc = sh->idle = p;
        cpus[i].sched = sh;
    }
}

static void simple_sched()
{
    setup_checker(0);
    struct proc* this = thisproc(), * next = NULL;
    bool goto_idle = panic_flag;
    acquire_spinlock(0, &proc_list_lock);
    if (!goto_idle)
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
            else if (goto_idle)
            {
                this->state = RUNNABLE;
                next = cpus[cpuid()].sched->idle;
            }
            break;
        case IDLE:
            if (next)
                next->state = RUNNING;
            break;
    }
    if (next)
    {
        reset_clock(RR_TIME);
        cpus[cpuid()].sched->proc = next;
        swtch(next->kcontext, &this->kcontext);
    }
    release_spinlock(0, &proc_list_lock);
}

__attribute__((weak, alias("simple_sched"))) void sched();

NO_RETURN void idle_entry()
{
    set_cpu_on();
    while (1)
    {
        sched();
        if (panic_flag)
            break;
        arch_with_trap
        {
            arch_wfi();
        }
    }
    set_cpu_off();
    arch_stop_cpu();
}

NO_INLINE NO_RETURN void _panic(const char* file, int line)
{
    printk("=====PANIC!=====\n");
    panic_flag = true;
    set_cpu_off();
    for (int i = 0; i < NCPU; i++)
    {
        if (cpus[i].online)
            i--;
    }
    printk("Kernel PANIC invoked at %s:%d. Stopped.\n", file, line);
    arch_stop_cpu();
}
