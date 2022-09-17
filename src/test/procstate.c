#include <kernel/sched.h>
#include <common/sem.h>
#include <test/test.h>
#include <kernel/mem.h>
#include <kernel/printk.h>

static Semaphore e0;

static void proc_test_main(u64 arg)
{
    ASSERT(wait_sem(&e0));
    exit(arg);
}

void proc_test()
{
    init_sem(&e0, 0);
    for (int i = 0; i < 10; i++)
    {
        struct proc* p = kalloc(sizeof *p);
        init_proc(p);
        start_proc(p, proc_test_main, i);
    }
    for (int i = 0; i < 10; i++)
    {
        post_sem(&e0);
    }
    while (1)
    {
        int ex;
        int ch = wait(&ex);
        if (ch == -1)
            break;
        printk("pid %d exit with code %d\n", ch, ex);
    }
    printk("proc_test PASS\n");
}
