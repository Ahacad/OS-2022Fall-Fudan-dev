#include <common/string.h>
#include <common/list.h>
#include <kernel/container.h>
#include <kernel/init.h>
#include <kernel/printk.h>
#include <kernel/mem.h>
#include <kernel/sched.h>

struct container root_container;
extern struct proc root_proc;

void set_container_parent_to_this(struct container* container) {
    container->parent = thisproc()->container;
}

void set_proc_to_container(struct proc* p, struct container* c) {
    p->container = c;
}
void set_proc_container_to_this(struct proc* proc) {
    proc->container = thisproc()->container;
}
void _set_container_parent(struct container* container, struct container* parent) {
    container->parent = parent;
}


struct container* create_container() {
    struct proc* rootproc = create_proc();
    struct container* c = kalloc(sizeof(struct container));
    memset(c, 0, sizeof(*c));
    c->rootproc = rootproc;
    c->max_pid = 0;
    return c;
}

define_early_init(root_container) {
    root_container.max_pid = 0;
    root_container.parent = &root_container;
    root_container.rootproc = &root_proc;
    init_schqueue(&root_container.schqueue);
    init_schinfo(&root_container.schinfo, true);
}
