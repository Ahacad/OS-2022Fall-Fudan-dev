#include <common/event.h>
#include <kernel/sched.h>
#include <kernel/printk.h>

void init_event(Event* evt, bool signalled)
{
    evt->signalled = signalled;
    init_spinlock(&evt->lock);
    init_list_node(&evt->sleeplist);
}

bool wait_for_event(Event* evt)
{
    bool ret;
    setup_checker(0);
    acquire_spinlock(0, &evt->lock);
    if (evt->signalled)
    {
        evt->signalled = false;
        release_spinlock(0, &evt->lock);
        return true;
    }
    _insert_into_list(&evt->sleeplist, &thisproc()->slnode);
    while (1)
    {
        lock_for_sched(0);
        release_spinlock(0, &evt->lock);
        sched(0, SLEEPING);
        acquire_spinlock(0, &evt->lock);
        if (evt->signalled)
        {
            evt->signalled = false;
            ret = true;
            break;
        }
        else if (thisproc()->killed)
        {
            ret = false;
            break;
        }
    }
    _detach_from_list(&thisproc()->slnode);
    release_spinlock(0, &evt->lock);
    return ret;
}

void set_event(Event* evt)
{
    setup_checker(0);
    acquire_spinlock(0, &evt->lock);
    evt->signalled = true;
    if (!_empty_list(&evt->sleeplist))
    {
        auto towake = container_of(evt->sleeplist.prev, struct proc, slnode);
        activate_proc(towake);
    }
    release_spinlock(0, &evt->lock);
}
