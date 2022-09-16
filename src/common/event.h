#pragma once

#include <common/list.h>

typedef struct
{
    bool signalled;
    SpinLock lock;
    ListNode sleeplist;
} Event;

void init_event(Event*, bool);
bool wait_for_event(Event*);
void set_event(Event*);

#define SleepLock Event
#define init_sleeplock(lock) init_event(lock, true)
#define acquire_sleeplock(checker, lock) (checker_begin_ctx(checker), wait_for_event(lock))
#define release_sleeplock(checker, lock) (checker_end_ctx(checker), set_event(lock))
