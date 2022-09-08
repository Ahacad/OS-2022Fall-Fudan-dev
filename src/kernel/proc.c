#include <kernel/proc.h>
#include <kernel/init.h>
#include <kernel/mem.h>
#include <kernel/sched.h>
#include <common/list.h>
#include <kernel/printk.h>

SpinLock proc_list_lock;
struct proc root_proc;
static SpinLock max_pid_lock;
static int max_pid;

void kernel_entry();

NO_INLINE static u64 proc_entry(void(*entry)(u64), u64 arg)
{
    _release_spinlock(&proc_list_lock);
    u64* fp = __builtin_frame_address(0);
    fp[1] = (u64)entry;
    return arg;
}

void start_proc(struct proc* p, void(*entry)(u64), u64 arg)
{
    setup_checker(0);
    p->kcontext->lr = (u64)&proc_entry;
    p->kcontext->x0 = (u64)entry;
    p->kcontext->x1 = (u64)arg;
    p->state = RUNNABLE;
    acquire_spinlock(0, &proc_list_lock);
    _insert_into_list(&root_proc.list, &p->list);
    release_spinlock(0, &proc_list_lock);
}

void init_proc(struct proc* p)
{
    setup_checker(0);
    p->state = UNUSED;
    p->kstack = kalloc_page();
    p->kcontext = (KernelContext*)((u64)p->kstack + PAGE_SIZE - 16 - sizeof(KernelContext));
    p->ucontext = NULL;
    acquire_spinlock(0, &max_pid_lock);
    p->pid = ++max_pid;
    release_spinlock(0, &max_pid_lock);
}

define_early_init(proc_list)
{
    setup_checker(0);
    init_spinlock(0, &proc_list_lock);
    init_list_node(&root_proc.list);
    root_proc.pid = 0;
    root_proc.state = UNUSED;
}

define_init(root_proc)
{
    init_proc(&root_proc);
    start_proc(&root_proc, kernel_entry, 123456);
}
