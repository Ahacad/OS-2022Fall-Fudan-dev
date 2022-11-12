#include <kernel/proc.h>
#include <kernel/container.h>
#include <kernel/printk.h>
#include <kernel/sched.h>
#include <test/test.h>

void trap_return();

static void dummy() {
    exit(0);
}

void container_test() {
    // THERE IS BUG WITH semaphores and processes here
    printk("container_test\n");
    for (int i = 1; i <= 12; i++) {
        auto p = create_proc();
        int pid = start_proc(p, dummy, 0);
        printk("root container %d process %d pid = %d\n", thisproc()->container->rootproc->pid, i, pid);
    }
    auto c = create_container();
    set_container_parent_to_this(c);
    for (int i = 1; i <= 12; i++) {
        auto p = create_proc();
        set_proc_to_container(p, c);
        int pid = start_proc(p, dummy, 0);
        printk("proc %d in container %d, localpid %d\n", pid, c->rootproc->pid, p->localpid);
        
    }

    printk("container_test finish\n");
}
