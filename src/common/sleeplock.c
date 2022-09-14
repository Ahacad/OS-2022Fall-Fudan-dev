#include <common/sleeplock.h>
#include <kernel/sched.h>
#include <kernel/printk.h>

void init_sleeplock(SleepLock* lock)
{
    lock->locked = false;
    init_spinlock(&lock->lock);
    init_list_node(&lock->sleeplist);
}

bool _acquire_sleeplock(SleepLock* lock)
{
    setup_checker(0);
    auto this = thisproc();
    acquire_spinlock(0, &lock->lock);
    if (!lock->locked)
    {
        lock->locked = true;
        release_spinlock(0, &lock->lock);
        return true;
    }
    _insert_into_list(&lock->sleeplist, &this->slnode);
    lock_for_sched(0);
    release_spinlock(0, &lock->lock);
    sched(0, SLEEPING);
    acquire_spinlock(0, &lock->lock);
    _detach_from_list(&this->slnode);
    if (!lock->locked)
    {
        lock->locked = true;
        release_spinlock(0, &lock->lock);
        return true;
    }
    else
    {
        printk("wakeup by killed?\n");
        release_spinlock(0, &lock->lock);
        return false;
    }
}

void _release_sleeplock(SleepLock* lock)
{
    setup_checker(0);
    acquire_spinlock(0, &lock->lock);
    if (_empty_list(&lock->sleeplist))
    {
        lock->locked = false;
    }
    else
    {
        auto towake = container_of(lock->sleeplist.next, struct proc, slnode);
        activate_proc(towake);
    }
    release_spinlock(0, &lock->lock);
}
