#pragma once

#include <common/list.h>

typedef struct
{
    bool locked;
    SpinLock lock;
    ListNode sleeplist;
} SleepLock;

bool _acquire_sleeplock(SleepLock*);
void _release_sleeplock(SleepLock*);

void init_sleeplock(SleepLock*);
#define acquire_sleeplock(checker, lock) (checker_begin_ctx(checker), _acquire_sleeplock(lock))
#define release_sleeplock(checker, lock) (checker_end_ctx(checker), _release_sleeplock(lock))
