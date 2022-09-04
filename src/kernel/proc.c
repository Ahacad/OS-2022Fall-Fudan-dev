#include <kernel/proc.h>
#include <kernel/init.h>
#include <kernel/mem.h>
#include <kernel/sched.h>

static SpinLock proc_list_lock;
static int maxpid;
struct proc root_proc;

void kernel_entry();

static void kernel_proc_entry(void(*entry)(u64), u64 arg)
{

}

void init_proc(struct proc* p)
{
    setup_checker(0);
    p->state = UNUSED;
    p->kstack = kalloc_page();
    p->kcontext = (KernelContext*)((u64)p->kstack + PAGE_SIZE - 16 - sizeof(KernelContext));
    p->ucontext = NULL;
    acquire_spinlock(0, &proc_list_lock);
    p->pid = ++maxpid;
    _insert_into_list(&root_proc.list, &p->list);
    release_spinlock(0, &proc_list_lock);
}

void update_proc_state(struct proc* proc, enum procstate state)
{
    setup_checker(0);
    acquire_spinlock(0, &proc_list_lock);
    proc->state = state;
    release_spinlock(0, &proc_list_lock);
}

static void init_proc_list()
{
    setup_checker(0);
    init_spinlock(0, &proc_list_lock);
    init_list_node(&root_proc.list);
    root_proc.pid = 0;
    root_proc.state = UNUSED;
}
early_init_func(init_proc_list);

static void init_root_proc()
{
    init_proc(&root_proc);
    root_proc.kcontext->lr = (u64)kernel_entry;
    root_proc.state = RUNNABLE;
}
init_func(init_root_proc);
