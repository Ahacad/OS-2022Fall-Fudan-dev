#include "common/list.h"
#include "common/spinlock.h"
#include "common/string.h"
#include "kernel/init.h"
#include "kernel/mem.h"
#include "kernel/printk.h"
struct slab {
    ListNode node;
    u16 idx;
    u16 top;
    char data[];
};
struct kmem_cache {
    SpinLock lock;
    int objectSize;
    int objectNum;
    ListNode partialList, fullList;
    int freelistOffset;
    int objectOffset;
    void *(*alloc)(struct kmem_cache *this);
    void (*free)(struct kmem_cache *this, void *p);
};
static struct kmem_cache kmalloc_[12];
static void init_slab(struct kmem_cache *kmem, struct slab *p) {
    memset(p, 0, PAGE_SIZE);
    p->idx = (u16)(kmem - kmalloc_);
    for (int i = 0; i < kmem->objectNum; i++) {
        ((u16 *)((u64)p + (u64)kmem->freelistOffset))[i] = i;
    }
}
void *kmem_alloc(struct kmem_cache *this) {
    setup_checker(0);
    acquire_spinlock(0, &this->lock);
    struct slab *slabp = NULL;
    if (this->partialList.next == &this->partialList) {
        slabp = (struct slab *)kalloc_page();
        init_slab(this, slabp);
        _insert_into_list(&this->partialList, &slabp->node);
    }
    slabp = container_of(this->partialList.next, struct slab, node);
    void *rtn = (void *)(u64)((u64)slabp +
                              ((u16 *)((u64)this->freelistOffset +
                                       (u64)slabp))[slabp->top] *
                                  this->objectSize +
                              this->objectOffset);
    slabp->top++;
    if (slabp->top >= this->objectNum) {
        // move slabp to fullList
        _detach_from_list(&slabp->node);
        _insert_into_list(&this->fullList, &slabp->node);
    }
    release_spinlock(0, &this->lock);
    return rtn;
}
void kmem_free(struct kmem_cache *this, void *p) {
    setup_checker(0);
    acquire_spinlock(0, &this->lock);
    void *pg = (void *)((u64)p >> 12 << 12);
    struct slab *slabp = (struct slab *)pg;
    slabp->top--;
    ((u16 *)((u64)this->freelistOffset + (u64)pg))[slabp->top] =
        ((u64)p - (u64)pg - this->objectOffset) / this->objectSize;
    // move to partialList
    _detach_from_list(&slabp->node);
    _insert_into_list(&this->partialList, &slabp->node);
    if (slabp->top == 0) {
        // free slab,must in partialList
        _detach_from_list(&slabp->node);
        kfree_page(pg);
    }
    release_spinlock(0, &this->lock);
}
static void init_kmem_cache(int idx) {
    init_spinlock(&kmalloc_[idx].lock);
    init_list_node(&kmalloc_[idx].partialList);
    init_list_node(&kmalloc_[idx].fullList);
    kmalloc_[idx].alloc = kmem_alloc;
    kmalloc_[idx].free = kmem_free;
    kmalloc_[idx].freelistOffset = offset_of(struct slab, data);
    kmalloc_[idx].objectSize = 1 << idx;
    int maxNum = (PAGE_SIZE - kmalloc_[idx].freelistOffset) /
                 (2 + kmalloc_[idx].objectSize);
    int objOffset = maxNum * 2 + kmalloc_[idx].freelistOffset;
    if (idx == 0 || idx == 1)
        ;
    else if (idx == 2) {
        if (objOffset % 4) {
            objOffset = ((objOffset >> 2) << 2) + 4;
            if (objOffset + maxNum * kmalloc_[idx].objectSize > PAGE_SIZE)
                maxNum -= 1;
        }
    } else {
        if (objOffset % 8) {
            objOffset = ((objOffset >> 3) << 3) + 8;
            if (objOffset + maxNum * kmalloc_[idx].objectSize > PAGE_SIZE)
                maxNum -= 1;
        }
    }
    kmalloc_[idx].objectNum = maxNum;
    kmalloc_[idx].objectOffset = objOffset;
}
void init_kmem() {
    for (int i = 0; i < 12; i++)
        init_kmem_cache(i);
}
void *kalloc(isize sz) {
    if (sz > PAGE_SIZE / 2 || sz <= 0) return NULL;
    for (int i = 0;; i++) {
        if ((1 << i) >= sz) return kmalloc_[i].alloc(&kmalloc_[i]);
    }
}
void kfree(void *p) {
    void *pg = (void *)((u64)p >> 12 << 12);
    struct slab *slabp = (struct slab *)pg;
    int idx = slabp->idx;
    kmalloc_[idx].free(&kmalloc_[idx], p);
}