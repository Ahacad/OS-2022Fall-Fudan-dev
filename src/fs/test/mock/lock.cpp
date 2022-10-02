#include "lock_config.hpp"
#include "map.hpp"

#include <condition_variable>
#include <semaphore.h>
#include <cassert>
namespace {

struct Mutex {
    bool locked;
    std::mutex mutex;

    void lock() {
        mutex.lock();
        locked = true;
    }

    void unlock() {
        locked = false;
        mutex.unlock();
    }
};

struct Signal {
    // use a pointer to avoid `pthread_cond_destroy` blocking process exit.
    std::condition_variable_any* cv;

    Signal() {
        cv = new std::condition_variable_any;
    }
};

Map<void*, Mutex> mtx_map;
Map<void*, Signal> sig_map;

}  // namespace

extern "C" {
void init_spinlock(struct SpinLock* lock, const char* name [[maybe_unused]]) {
    mtx_map.try_add(lock);
}

void _acquire_spinlock(struct SpinLock* lock) {
    mtx_map[lock].lock();
}

void _release_spinlock(struct SpinLock* lock) {
    mtx_map[lock].unlock();
}

bool holding_spinlock(struct SpinLock* lock) {
    return mtx_map[lock].locked;
}

void init_sleeplock(struct SleepLock* lock, const char* name [[maybe_unused]]) {
    mtx_map.try_add(lock);
}

void acquire_sleeplock(struct SleepLock* lock) {
    mtx_map[lock].lock();
}

void release_sleeplock(struct SleepLock* lock) {
    mtx_map[lock].unlock();
}

void _fs_test_sleep(void* chan, struct SpinLock* lock) {
    sig_map.safe_get(chan).cv->wait(mtx_map[lock].mutex);
}

void _fs_test_wakeup(void* chan) {
    sig_map.safe_get(chan).cv->notify_all();
}

struct Semaphore;
void init_sem(Semaphore* x, int val) {
    sem_init((sem_t*)x, 0, val);
}
void post_sem(Semaphore* x) {
    sem_post((sem_t*)x);
}
bool wait_sem(Semaphore* x) {
    // int t = time(NULL);
    // while (1)
    // {
    //     if (!sem_trywait((sem_t*)x))
    //         break;
    //     if (time(NULL) - t > 3)
    //     {
    //         printf("%p\n", x);
    //         assert(0);
    //     }
    // }
    sem_wait((sem_t*)x);
    return true;
}
void wait_for_sem_locked(Semaphore* x) {
    int t = time(NULL);
    while (1)
    {
        if (sem_trywait((sem_t*)x))
            break;
        sem_post((sem_t*)x);
        if (time(NULL) - t > 3)
        {
            printf("%p\n", x);
            assert(0);
        }
    }
}
}
