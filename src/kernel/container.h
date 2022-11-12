#pragma once

#include <kernel/proc.h>
#include <kernel/schinfo.h>


struct container
{
    struct container* parent;
    struct proc* rootproc;

    struct schinfo schinfo;
    struct schqueue schqueue;

    // TODO: namespace
    int max_pid;
};

void set_container_parent_to_this(struct container*);
void set_proc_container_to_this(struct proc*);
void set_proc_to_container(struct proc*, struct container*);
struct container* create_container();
