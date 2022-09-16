#include <kernel/sched.h>
#include <common/event.h>
#include <test/test.h>
#include <kernel/mem.h>
#include <kernel/printk.h>

static Event e0, e1, end;

static void proc_test_main(u64 id)
{
    ASSERT(wait_for_event(&e0));
    printk("%llu\n", id);
    set_event(&e1);
    wait_for_event(&end);
}

void proc_test()
{
    init_event(&e0, false);
    init_event(&e1, false);
    for (int i = 0; i < 10; i++)
    {
        struct proc* p = kalloc(sizeof *p);
        init_proc(p);
        start_proc(p, proc_test_main, i);
    }
    for (int i = 0; i < 10; i++)
    {
        printk("%d ", i);
        set_event(&e0);
        ASSERT(wait_for_event(&e1));
    }
}
