#include <kernel/sched.h>
#include <kernel/proc.h>
#include <kernel/init.h>
#include <kernel/mem.h>
#include <kernel/printk.h>
#include <aarch64/intrinsic.h>
#include <kernel/cpu.h>
#include <driver/clock.h>
#include <test/test.h>

extern bool panic_flag;

static SpinLock sched_lock;
static ListNode sched_queue;

extern void swtch(KernelContext* new_ctx, KernelContext** old_ctx);

struct proc* thisproc()
{
    return cpus[cpuid()].sched.proc;
}

define_early_init(squeue)
{
    init_spinlock(&sched_lock);
    init_list_node(&sched_queue);
}

define_init(sched)
{
    for (int i = 0; i < NCPU; i++)
    {
        struct proc* p = kalloc(sizeof(struct proc));
        p->pid = 0;
        p->state = RUNNING;
        p->idle = true;
        cpus[i].sched.proc = cpus[i].sched.idle = p;
        cpus[i].sched.curr = &sched_queue;
    }
}

void init_schinfo(struct schinfo* p)
{
    init_list_node(&p->sqnode);
}

void _acquire_sched_lock()
{
    _acquire_spinlock(&sched_lock);
}

void _release_sched_lock()
{
    _release_spinlock(&sched_lock);
}

bool is_zombie(struct proc* p)
{
    bool r;
    _acquire_sched_lock();
    r = p->state == ZOMBIE;
    _release_sched_lock();
    return r;
}

void activate_proc(struct proc* p)
{
    _acquire_sched_lock();
    if (p->state == UNUSED || p->state == SLEEPING)
    {
        p->state = RUNNABLE;
        _insert_into_list(&sched_queue, &p->schinfo.sqnode);
    }
    _release_sched_lock();
}

static void update_this_state(enum procstate new_state)
{
    switch (new_state)
    {
    case RUNNABLE:
        break;
    case SLEEPING: case ZOMBIE:
        cpus[cpuid()].sched.curr = cpus[cpuid()].sched.curr->next;
        _detach_from_list(&thisproc()->schinfo.sqnode);
        break;
    default:
        PANIC();
    }
    thisproc()->state = new_state;
}

static struct proc* pick_next()
{
    auto curr = cpus[cpuid()].sched.curr;
    if (panic_flag)
        return cpus[cpuid()].sched.idle;
    _for_in_list(p, curr)
    {
        if (p == &sched_queue)
            continue;
        auto proc = container_of(p, struct proc, schinfo.sqnode);
        if (proc->state == RUNNING)
            continue;
        if (proc->state == RUNNABLE)
        {
            cpus[cpuid()].sched.curr = &proc->schinfo.sqnode;
            return proc;
        }
        else
            PANIC(); // WTF?
    }
    cpus[cpuid()].sched.curr = &sched_queue;
    return cpus[cpuid()].sched.idle;
}

static void update_this_proc(struct proc* p)
{
    cpus[cpuid()].sched.proc = p;
    if (p->idle)
    {
        reset_clock(1); // give more wakeups to wfi()...
    }
    else
    {
        reset_clock(RR_TIME);
    }
}

static void simple_sched(enum procstate new_state)
{
    auto this = thisproc();
    ASSERT(this->state == RUNNING);
    update_this_state(new_state);
    auto next = pick_next();
    // printk("[%d %d->%d]\n", cpuid(), this->pid, next->pid);
    update_this_proc(next);
    ASSERT(next->state == RUNNABLE);
    next->state = RUNNING;
    if (next != this)
    {
        swtch(next->kcontext, &this->kcontext);
    }
    _release_sched_lock();
}

__attribute__((weak, alias("simple_sched"))) void _sched(enum procstate new_state);

u64 proc_entry(void(*entry)(u64), u64 arg)
{
    _release_sched_lock();
    set_return_addr(entry);
    return arg;
}

