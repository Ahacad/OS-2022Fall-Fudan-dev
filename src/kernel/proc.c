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

static void _set_proc_parent(struct proc* proc, struct proc* parent)
{
    _insert_into_list(&parent->children, &proc->ptnode);
    proc->parent = parent;
}

void set_parent_to_this(struct proc* proc)
{
    setup_checker(0);
    acquire_spinlock(0, &proc_list_lock);
    _set_proc_parent(proc, thisproc());
    release_spinlock(0, &proc_list_lock);
}

NO_RETURN void exit(int code)
{
    auto this = thisproc();
    ASSERT(this != &root_proc && !this->idle);
    setup_checker(0);
    // cleanup resources
    // TODO
    this->exitcode = code;
    // transfer children to the root_proc
    acquire_spinlock(0, &proc_list_lock);
    auto cl = _detach_from_list(&this->children);
    if (cl)
    {
        _for_in_list(p, cl)
        {
            auto proc = container_of(p, struct proc, ptnode);
            proc->parent = &root_proc;
        }
        _merge_list(&root_proc.children, cl);
    }
    // notify the root_proc if there is zombie in children
    while (get_sem(&this->childexit))
        post_sem(&root_proc.childexit);
    // notify the parent
    post_sem(&this->parent->childexit);
    // go die
    // unlock the proc_list after locking the sched to ensure state=ZOMBIE when the parent querying
    lock_for_sched(0);
    release_spinlock(0, &proc_list_lock);
    sched(0, ZOMBIE);
    while (1); // hack gcc
}

int wait(int* exitcode)
{
    auto this = thisproc();
    ASSERT(!this->idle);
    setup_checker(0);
    // return -1 if no children
    acquire_spinlock(0, &proc_list_lock);
    if (_empty_list(&this->children))
    {
        release_spinlock(0, &proc_list_lock);
        return -1;
    }
    release_spinlock(0, &proc_list_lock);
    // wait for childexit
    if (!wait_sem(&this->childexit))
        return -1;
    // cleanup the child
    acquire_spinlock(0, &proc_list_lock);
    _for_in_list(p, &this->children)
    {
        if (p == &this->children)
            continue;
        auto proc = container_of(p, struct proc, ptnode);
        if (is_zombie(proc))
        {
            _detach_from_list(p);
            release_spinlock(0, &proc_list_lock);
            int pid = proc->pid;
            *exitcode = proc->exitcode;
            kfree_page(proc->kstack);
            kfree(proc);
            return pid;
        }
    }
    PANIC(); // ???
}

int start_proc(struct proc* p, void(*entry)(u64), u64 arg)
{
    if (p->parent == NULL)
    {
        setup_checker(0);
        acquire_spinlock(0, &proc_list_lock);
        _set_proc_parent(p, &root_proc);
        release_spinlock(0, &proc_list_lock);
    }
    p->kcontext->lr = (u64)&proc_entry;
    p->kcontext->x0 = (u64)entry;
    p->kcontext->x1 = (u64)arg;
    int pid = p->pid;
    activate_proc(p);
    return pid;
}

void init_proc(struct proc* p)
{
    setup_checker(0);
    memset(p, 0, sizeof(*p));
    p->state = UNUSED;
    p->kstack = kalloc_page();
    p->kcontext = (KernelContext*)((u64)p->kstack + PAGE_SIZE - 16 - sizeof(KernelContext) - sizeof(UserContext));
    p->ucontext = (UserContext*)((u64)p->kstack + PAGE_SIZE - 16 - sizeof(UserContext));
    init_sem(&p->childexit, 0);
    init_list_node(&p->children);
    init_list_node(&p->ptnode);
    p->parent = NULL;
    init_schinfo(&p->schinfo);
    acquire_spinlock(0, &proc_list_lock);
    p->pid = ++max_pid;
    release_spinlock(0, &proc_list_lock);
}

struct proc* create_proc()
{
    struct proc* p = kalloc(sizeof(struct proc));
    init_proc(p);
    return p;
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
