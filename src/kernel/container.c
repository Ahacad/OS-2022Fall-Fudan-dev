#include <kernel/container.h>
#include <kernel/init.h>
#include <kernel/sched.h>

struct container root_container;
extern struct proc root_proc;

define_early_init(root_container)
{
    root_container.parent = NULL;
    root_container.rootproc = &root_proc;
    init_schqueue(&root_container.schqueue);
    init_schinfo(&root_container.schinfo, true);
}
