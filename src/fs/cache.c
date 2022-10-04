#include <common/bitmap.h>
#include <common/string.h>
#include <kernel/mem.h>
#include <kernel/proc.h>
#include <kernel/printk.h>
#include <fs/cache.h>

static const SuperBlock* sblock;
static const BlockDevice* device;

static SpinLock lock;  // protects block cache.
static ListNode head;  // the list of all allocated in-memory block.
static LogHeader header;  // in-memory copy of log header block.

// hint: you may need some other variables. Just add them here.
struct LOG {
    /* data */
    SpinLock lock;
    Semaphore sem, outstandingsem;
    int outstanding;
    int committing;
    int mu;
    int mx;
} log;

// read the content from disk.
static INLINE void device_read(Block* block) {
    device->read(block->block_no, block->data);
}

// write the content back to disk.
static INLINE void device_write(Block* block) {
    device->write(block->block_no, block->data);
}

// read log header from disk.
static INLINE void read_header() {
    device->read(sblock->log_start, (u8*)&header);
}

// write log header back to disk.
static INLINE void write_header() {
    device->write(sblock->log_start, (u8*)&header);
}

// initialize a block struct.
static void init_block(Block* block) {
    block->block_no = 0;
    init_list_node(&block->node);
    block->acquired = false;
    block->pinned = false;

    init_sleeplock(&block->lock);
    // printf("%d\n", (void*)(block->data) - (void*)block);
    block->valid = false;
    memset(block->data, 0, sizeof(block->data));
}

// see `cache.h`.
static usize get_num_cached_blocks() {
    // TODO
    ListNode* p = head.next;
    usize ret = 0;
    while (p != &head) {
        p = p->next;
        ret++;
    }
    return ret;
}

// see `cache.h`.
static Block* cache_acquire(usize block_no) {
    // TODO
    _acquire_spinlock(&lock);
    bool IsTheBlockInCache = false;
    ListNode* p = head.next;
    while (p != &head) {
        Block* b = container_of(p, Block, node);
        if (b->block_no == block_no) {
            IsTheBlockInCache = true;
            break;
        }
        p = p->next;
    }

    if (IsTheBlockInCache) {
        _detach_from_list(p);
        _merge_list(&head, p);
        Block* b = container_of(p, Block, node);
        _release_spinlock(&lock);
        setup_checker(0);
        acquire_sleeplock(0, &b->lock);
        checker_end_ctx(0);
        return b;
    }
    usize sz = get_num_cached_blocks();
    if (sz >= EVICTION_THRESHOLD) {
        ListNode* q = head.prev;
        while (sz >= EVICTION_THRESHOLD && q != &head) {
            Block* b = container_of(q, Block, node);
            if (b->pinned == false && b->acquired == false) {
                ListNode* t = _detach_from_list(q);
                kfree(b);
                q = t;
                sz--;
            } else
                q = q->prev;
        }
    }
    Block* b = kalloc(sizeof(Block));
    init_block(b);
    p = &b->node;
    _merge_list(&head, p);
    device->read(block_no, b->data);
    b->block_no = block_no;
    b->valid = 1;
    b->acquired = 1;
    _release_spinlock(&lock);
    setup_checker(0);
    acquire_sleeplock(0, &b->lock);
    checker_end_ctx(0);
    return b;
}

// see `cache.h`.
static void cache_release(Block* block) {
    // TODO
    block->acquired = false;
    setup_checker(0);
    checker_begin_ctx(0);
    release_sleeplock(0, &block->lock);
}

void install_trans(int recovering) {
    for (u32 tail = 0; tail < header.num_blocks; tail++) {
        Block* lbuf = cache_acquire((usize)(sblock->log_start + tail + 1));
        Block* dbuf = cache_acquire((usize)(header.block_no[tail]));
        memmove(dbuf->data, lbuf->data, BLOCK_SIZE);
        device_write(dbuf);
        if (recovering)
            dbuf->pinned = 0;
        cache_release(lbuf);
        cache_release(dbuf);
    }
}

void recover_from_log() {
    read_header();
    install_trans(1);
    header.num_blocks = 0;
    write_header();
}

// initialize block cache.
void init_bcache(const SuperBlock* _sblock, const BlockDevice* _device) {
    sblock = _sblock;
    device = _device;

    // TODO

    init_list_node(&head);
    init_spinlock(&lock);

    init_spinlock(&log.lock);
    init_sem(&log.sem, 0);
    init_sem(&log.outstandingsem, 0);
    log.mu = 0;
    log.mx = MIN(sblock->num_log_blocks - 1, LOG_MAX_SIZE);
    recover_from_log();
}

// see `cache.h`.
static void cache_begin_op(OpContext* ctx) {
    // TODO
    while (1) {
        setup_checker(0);
        acquire_spinlock(0, &log.lock);
        if (log.committing) {
            delayed_wait_sem(0, &log.sem);
            release_spinlock(0, &log.lock);
        } else if (header.num_blocks + log.mu + OP_MAX_NUM_BLOCKS > log.mx) {
            delayed_wait_sem(0, &log.sem);
            release_spinlock(0, &log.lock);
        } else {
            log.outstanding++;
            log.mu += OP_MAX_NUM_BLOCKS;
            ctx->rm = OP_MAX_NUM_BLOCKS;

            release_spinlock(0, &log.lock);
            break;
        }
    }
}

// see `cache.h`.
static void cache_sync(OpContext* ctx, Block* block) {
    if (ctx) {
        // TODO
        _acquire_spinlock(&log.lock);
        if (header.num_blocks >= log.mx) {
            PANIC();
        }
        if (log.outstanding < 1)
            PANIC();
        usize cnt = header.num_blocks, i;
        for (i = 0; i < cnt; i++) {
            if (header.block_no[i] == block->block_no) {
                break;
            }
        }
        header.block_no[i] = block->block_no;
        if (i == header.num_blocks) {
            header.num_blocks++;
            block->pinned = 1;
            if (ctx->rm > 0) {
                ctx->rm--;
                log.mu--;
            } else {
                PANIC();
            }
        }
        _release_spinlock(&log.lock);
    } else
        device_write(block);
}
void write_log() {
    for (u32 tail = 0; tail < header.num_blocks; tail++) {
        Block* from = cache_acquire(header.block_no[tail]);
        Block* to = cache_acquire(sblock->log_start + tail + 1);
        memmove(to->data, from->data, BLOCK_SIZE);
        device_write(to);
        cache_release(from);
        cache_release(to);
    }
}
void commit() {
    if (header.num_blocks > 0) {
        write_log();
        write_header();
        install_trans(0);
        header.num_blocks = 0;
        write_header();
    }
}
// see `cache.h`.
static void cache_end_op(OpContext* ctx) {
    // TODO
    int do_commit = 0;
    setup_checker(0);
    acquire_spinlock(0, &log.lock);
    log.outstanding--;
    log.mu -= ctx->rm;
    if (log.committing)
        PANIC();
    if (log.outstanding == 0) {
        do_commit = 1, log.committing = 1;
        release_spinlock(0, &log.lock);
    }
    else {
        post_all_sem(&log.sem);
        delayed_wait_sem(0, &log.outstandingsem);
        release_spinlock(0, &log.lock);
    }
    if (do_commit) {
        commit();
        acquire_spinlock(0, &log.lock);
        log.committing = 0;
        post_all_sem(&log.outstandingsem);
        post_all_sem(&log.sem);
        release_spinlock(0, &log.lock);
    }
}

// see `cache.h`.
// hint: you can use `cache_acquire`/`cache_sync` to read/write blocks.
usize BBLOCK(usize b, SuperBlock* sb) {
    return b / BIT_PER_BLOCK + sb->bitmap_start;
}
void bzero(OpContext* ctx, u32 block_no) {
    Block* bp = cache_acquire(block_no);
    memset(bp->data, 0, BLOCK_SIZE);
    cache_sync(ctx, bp);
    cache_release(bp);
}
static usize cache_alloc(OpContext* ctx) {
    // TODO
    u32 b, bi, m;
    Block* bp = NULL;
    for (b = 0; b < sblock->num_blocks; b += BIT_PER_BLOCK) {
        bp = cache_acquire(BBLOCK((u32)b, sblock));
        for (bi = 0; bi < BIT_PER_BLOCK && b + bi < sblock->num_blocks; bi++) {
            m = (u32)(1 << (bi % 8));
            if ((bp->data[bi / 8] & (u8)m) == 0) {
                bp->data[bi / 8] |= (u8)m;
                cache_sync(ctx, bp);
                cache_release(bp);
                bzero(ctx, b + bi);
                return b + bi;
            }
        }
        cache_release(bp);
    }
    PANIC();
}

// see `cache.h`.
// hint: you can use `cache_acquire`/`cache_sync` to read/write blocks.
static void cache_free(OpContext* ctx, usize block_no) {
    // TODO
    Block* bp;
    int bi, m;
    bp = cache_acquire(BBLOCK(block_no, sblock));
    bi = block_no % BIT_PER_BLOCK;
    m = 1 << (bi % 8);
    if ((bp->data[bi / 8] & m) == 0) {
        PANIC();
    }
    bp->data[bi / 8] &= ~m;
    cache_sync(ctx, bp);
    cache_release(bp);
}

BlockCache bcache = {
    .get_num_cached_blocks = get_num_cached_blocks,
    .acquire = cache_acquire,
    .release = cache_release,
    .begin_op = cache_begin_op,
    .sync = cache_sync,
    .end_op = cache_end_op,
    .alloc = cache_alloc,
    .free = cache_free,
};
