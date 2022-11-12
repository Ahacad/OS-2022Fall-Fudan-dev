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
};

