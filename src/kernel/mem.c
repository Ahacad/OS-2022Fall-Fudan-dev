#include <common/list.h>
#include <common/string.h>
#include <kernel/mem.h>
#include <kernel/printk.h>
#include <kernel/init.h>
#include <driver/memlayout.h>

extern char end[];

static QueueNode* free_page;

define_init(pages)
{
    for (int i = (int)(((u64)&end + PAGE_SIZE - 1) / PAGE_SIZE); i < PHYSTOP / PAGE_SIZE; i++)
        kfree_page((void*)P2K(((u64)i) * PAGE_SIZE));
}

void* kalloc_page()
{
    return (void*)fetch_from_queue(&free_page);
}

void kfree_page(void* p)
{
    add_to_queue(&free_page, (QueueNode*)p);
}

static QueueNode* simple_alloc_pool[16];

static void* simple_pool_alloc(isize size)
{
    void* r = NULL;
    if (size > PAGE_SIZE - 8 || size <= 0)
        return NULL;
    if (size > PAGE_SIZE / 2 - 8)
        r = kalloc_page();
    unsigned i = 32 - (unsigned)__builtin_clz((unsigned)(size + 8 - 1)) - 4;
    r = fetch_from_queue(&simple_alloc_pool[i]);
    if (r == NULL)
    {
        r = kalloc_page();
        if (r == NULL)
            return NULL;
        unsigned s = 16u << i;
        for (unsigned o = s; o < PAGE_SIZE; o += s)
            add_to_queue(&simple_alloc_pool[i], (QueueNode*)((u64)r + o));
    }
    *(void**)r = &simple_alloc_pool[i];
    r = (void*)((u64)r + 8);
    memset(r, 0, (unsigned)size);
    return r;
}

static void simple_pool_free(void* p)
{
    add_to_queue(*(QueueNode***)((u64)p - 8), (QueueNode*)((u64)p - 8));
}

__attribute__((weak, alias("simple_pool_alloc"))) void* kalloc(isize);
__attribute__((weak, alias("simple_pool_free"))) void kfree(void*);
