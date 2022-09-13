#include <kernel/sched.h>
#include <kernel/proc.h>
#include <kernel/init.h>
#include <kernel/mem.h>
#include <kernel/printk.h>
#include <aarch64/intrinsic.h>
#include <kernel/cpu.h>
#include <driver/clock.h>

static bool panic_flag;

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
        p->state = IDLE;
        cpus[i].sched.proc = cpus[i].sched.idle = p;
    }
}

void init_schinfo(struct schinfo* p)
{
    init_list_node(&p->sqnode);
}

static struct proc* pick_next(struct proc* this)
{
    if (panic_flag)
        return cpus[cpuid()].sched.idle;
    _for_in_list(p, this->state == IDLE ? &sched_queue : &this->schinfo.sqnode)
    {
        if (p == &sched_queue)
            continue;
        auto proc = container_of(p, struct proc, schinfo.sqnode);
        if (proc->state == RUNNING)
            continue;
        if (proc->state == RUNNABLE)
        {
            return proc;
        }
        else
            PANIC(); // WTF?
    }
    return cpus[cpuid()].sched.idle;
}

static void update_sched_clock(struct proc* p)
{
    (void)p; // disable the 'unused' warning
    reset_clock(RR_TIME);
}

void activate_sched(struct proc* p)
{
    setup_checker(0);
    insert_into_list(0, &sched_lock, &sched_queue, &p->schinfo.sqnode);
}

void deactivate_sched(struct proc* p)
{
    setup_checker(0);
    detach_from_list(0, &sched_lock, &p->schinfo.sqnode);
}

static void simple_sched(enum procstate new_state)
{
    setup_checker(0);
    auto this = thisproc();
    acquire_spinlock(0, &sched_lock);
    if (this->state != IDLE)
    {
        ASSERT(this->state == RUNNING);
        switch (new_state)
        {
        case RUNNABLE:
            break;
        default:
            PANIC();
        }
        this->state = new_state;
    }
    auto next = pick_next(this);
    update_sched_clock(next);
    if (next->state != IDLE)
    {
        ASSERT(next->state == RUNNABLE);
        next->state = RUNNING;
    }
    if (next != this)
    {
        cpus[cpuid()].sched.proc = next;
        swtch(next->kcontext, &this->kcontext);
    }
    release_spinlock(0, &sched_lock);
}

__attribute__((weak, alias("simple_sched"))) void sched(enum procstate new_state);

u64 proc_entry(void(*entry)(u64), u64 arg)
{
    _release_spinlock(&sched_lock);
    u64* fp = __builtin_frame_address(0);
    fp[1] = (u64)entry;
    return arg;
}

#include <test/test.h>
NO_RETURN void idle_entry()
{
    alloc_test();
    set_cpu_on();
    while (1)
    {
        sched(IDLE);
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
