#include <kernel/proc.h>
#include <kernel/init.h>
#include <kernel/mem.h>
#include <kernel/sched.h>
#include <common/list.h>
#include <common/string.h>
#include <kernel/printk.h>

static SpinLock proc_list_lock;
struct proc root_proc;
static int max_pid;

void kernel_entry();
void proc_entry();

void reset_proc_parent(struct proc* proc, struct proc* parent)
{
    setup_checker(0);
    acquire_spinlock(0, &proc_list_lock);
    _insert_into_list(&parent->children, &proc->ptnode);
    proc->parent = parent;
    release_spinlock(0, &proc_list_lock);
}

void start_proc(struct proc* p, void(*entry)(u64), u64 arg)
{
    if (p->parent == NULL)
        reset_proc_parent(p, &root_proc);
    p->kcontext->lr = (u64)&proc_entry;
    p->kcontext->x0 = (u64)entry;
    p->kcontext->x1 = (u64)arg;
    activate_proc(p);
}

void init_proc(struct proc* p)
{
    setup_checker(0);
    memset(p, 0, sizeof(*p));
    p->state = UNUSED;
    p->kstack = kalloc_page();
    p->kcontext = (KernelContext*)((u64)p->kstack + PAGE_SIZE - 16 - sizeof(KernelContext));
    p->ucontext = NULL;
    init_list_node(&p->children);
    init_list_node(&p->ptnode);
    p->parent = NULL;
    init_schinfo(&p->schinfo);
    acquire_spinlock(0, &proc_list_lock);
    p->pid = ++max_pid;
    release_spinlock(0, &proc_list_lock);
}

define_early_init(proc_list)
{
    init_spinlock(&proc_list_lock);
}

define_init(root_proc)
{
    init_proc(&root_proc);
    root_proc.parent = &root_proc;
    start_proc(&root_proc, kernel_entry, 123456);
}
