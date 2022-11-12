#include <kernel/sched.h>
#include <kernel/proc.h>
#include <kernel/init.h>
#include <kernel/mem.h>
#include <kernel/printk.h>
#include <kernel/container.h>
#include <aarch64/intrinsic.h>
#include <kernel/cpu.h>
#include <test/test.h>

extern bool panic_flag;

static SpinLock sched_lock;
extern struct container root_container;

static struct timer sched_timer[NCPU];

static void sched_timer_handler(struct timer* t)
{
    ASSERT(t == &sched_timer[cpuid()] && t->triggered);
    yield();
}

static void reset_sched_timer(int ms)
{
    if (!sched_timer[cpuid()].triggered)
        cancel_cpu_timer(&sched_timer[cpuid()]);
    sched_timer[cpuid()].elapse = ms;
    set_cpu_timer(&sched_timer[cpuid()]);
}

extern void swtch(KernelContext* new_ctx, KernelContext** old_ctx);

struct proc* thisproc()
{
    return cpus[cpuid()].sched.proc;
}

define_early_init(slock)
{
    init_spinlock(&sched_lock);
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
        sched_timer[i].handler = sched_timer_handler;
        sched_timer[i].triggered = true;
    }
}

void init_schinfo(struct schinfo* p, bool group)
{
    p->group = group;
    init_list_node(&p->sqnode);
}

void init_schqueue(struct schqueue* q)
{
    init_list_node(&q->sqhead);
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

bool is_unused(struct proc* p)
{
    bool r;
    _acquire_sched_lock();
    r = p->state == UNUSED;
    _release_sched_lock();
    return r;
}

bool _activate_proc(struct proc* p, bool onalert)
{
    bool ret = false;
    _acquire_sched_lock();
    ASSERT(!p->idle);
    if (p->state == UNUSED || p->state == SLEEPING || (p->state == DEEPSLEEPING && !onalert))
    {
        p->state = RUNNABLE;
        _insert_into_list(&p->container->schqueue.sqhead, &p->schinfo.sqnode);
        ret = true;
    }
    _release_sched_lock();
    return ret;
}

static void update_this_state(enum procstate new_state)
{
    auto p = thisproc();
    switch (new_state)
    {
    case RUNNABLE:
        if (!p->idle)
            _insert_into_list(p->container->schqueue.sqhead.prev, &p->schinfo.sqnode);
        break;
    case SLEEPING: case ZOMBIE: case DEEPSLEEPING:
        break;
    default:
        PANIC();
    }
    thisproc()->state = new_state;
}

static struct proc* _pick_in_queue(struct schqueue* q)
{
    _for_in_list(p, &q->sqhead)
    {
        if (p == &q->sqhead)
            continue;
        auto info = container_of(p, struct schinfo, sqnode);
        if (info->group)
        {
            auto proc = _pick_in_queue(&container_of(info, struct container, schinfo)->schqueue);
            if (proc)
                return proc;
        }
        else
        {
            return container_of(info, struct proc, schinfo);
        }
    }
    return NULL;
}

static struct proc* pick_next()
{
    if (panic_flag)
        return cpus[cpuid()].sched.idle;
    auto p = _pick_in_queue(&root_container.schqueue);
    if (p)
    {
        ASSERT(p->state == RUNNABLE);
        _detach_from_list(&p->schinfo.sqnode);
        return p;
    }
    return cpus[cpuid()].sched.idle;
}

static void update_this_proc(struct proc* p)
{
    cpus[cpuid()].sched.proc = p;
    reset_sched_timer(10);
}

static void simple_sched(enum procstate new_state)
{
    auto this = thisproc();
    ASSERT(this->state == RUNNING);
    if (this->killed && new_state != ZOMBIE && new_state != DEEPSLEEPING)
    {
        _release_sched_lock();
        return;
    }
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

