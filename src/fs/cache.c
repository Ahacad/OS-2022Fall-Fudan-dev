#include <common/bitmap.h>
#include <common/string.h>
#include <kernel/mem.h>
#include <kernel/proc.h>
#include <kernel/sched.h>
#include <fs/cache.h>
#include <kernel/printk.h>
#include <common/spinlock.h>

static const SuperBlock* sblock;
static const BlockDevice* device;

static SpinLock lock;  // protects block cache.
static ListNode head;  // the list of all allocated in-memory block.
static LogHeader header;  // in-memory copy of log header block.

static SpinLock semlock, atomiclock;
#define beginpost _acquire_spinlock(&semlock)
#define endpost _release_spinlock(&semlock)
#define endwait _acquire_spinlock(&semlock), _release_spinlock(&semlock)

// hint: you may need some other variables. Just add them here.
struct LOG {
    /* data */
    SpinLock lock;
    int outstanding;
    int committing;
    int mu;
    int mx;
    Semaphore sem;

    Semaphore outstandsem;
    int stcnt;
    int logcnt;
} log;

static usize get_num_cached_blocks();
static Block* cache_acquire(usize block_no);
static void cache_release(Block* block);
static void cache_begin_op(OpContext* ctx);
static void cache_sync(OpContext* ctx, Block* block);
static void cache_end_op(OpContext* ctx);
static usize cache_alloc(OpContext* ctx);
static void cache_free(OpContext* ctx, usize block_no);

// read the content from disk.
static INLINE void device_read(Block* block) {
    device->read(block->block_no, block->data);
    // device->read(block->block_no + 0x20800, block->data);
}

// write the content back to disk.
static INLINE void device_write(Block* block) {
    device->write(block->block_no, block->data);
    // device->write(block->block_no + 0x20800, block->data);
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
    init_sem(&(block->sem), 1);
    block->valid = false;
    memset(block->data, 0, sizeof(block->data));
}

// for testing.
// get the number of cached blocks.
// or in other words, the number of allocated `Block` struct.
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

// read the content of block at `block_no` from disk, and lock the block.
// return the pointer to the locked block.
// hint: you may use kalloc,EVICTION_THRESHOLD here
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
        // printf("rel1 %d locked%d cpu%x thiscpu%x\n", block_no, lock.locked,
        //        lock.cpu, thiscpu());
        _release_spinlock(&lock);
        wait_sem(&b->sem);
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
    b->block_no = block_no;
    // printf("rel2 %d locked%d cpu%x thiscpu%x\n", block_no, lock.locked,
    //        lock.cpu, thiscpu());
    _release_spinlock(&lock);

    wait_sem(&b->sem);

    device_read(b);
    b->valid = 1;
    b->acquired = 1;

    return b;
}

// unlock `block`.
// NOTE: it does not need to write the block content back to disk.
static void cache_release(Block* block) {
    // TODO
    block->acquired = false;
    post_sem(&block->sem);
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

// initialize block cache and log.
// don't forget to recover from log in header
void init_bcache(const SuperBlock* _sblock, const BlockDevice* _device) {
    sblock = _sblock;
    device = _device;

    // TODO
    // printk("init bcache\n");
    // printk("%p %p\n", &(log.sem), &(log.outstandsem));
    init_spinlock(&atomiclock);
    init_list_node(&head);
    init_spinlock(&lock);
    init_spinlock(&log.lock);
    init_sem(&(log.sem), 0);
    init_sem(&(log.outstandsem), 0);
    log.mu = 0;
    log.mx = MIN(sblock->num_log_blocks - 1, LOG_MAX_SIZE);
    log.stcnt = 0;
    log.logcnt = 0;
    recover_from_log();
}

// begin a new atomic operation and initialize `ctx`.
// `OpContext` represents an outstanding atomic operation.

// wait until(no log is committing && enough block for logging)
// then reserve block for op
static void cache_begin_op(OpContext* ctx) {
    // TODO
    static int begin;
    _acquire_spinlock(&log.lock);
    // printk("BEGIN%d committing=%d\n", begin++, log.committing);
    while (1) {
        if (log.committing) {
            
            // printk("Va %d\n", log.logcnt);
            log.logcnt++;
            _release_spinlock(&log.lock);
            wait_sem(&log.sem);
            _acquire_spinlock(&log.lock);

        } else if ((int)header.num_blocks + log.mu + OP_MAX_NUM_BLOCKS > log.mx) {
            log.logcnt++;
            // printk("Vb %d\n", log.logcnt);
            _release_spinlock(&log.lock);
            wait_sem(&log.sem);
            _acquire_spinlock(&log.lock);

        } else {
            log.outstanding++;
            log.mu += OP_MAX_NUM_BLOCKS;
            ctx->rm = OP_MAX_NUM_BLOCKS;
            _release_spinlock(&log.lock);
            break;
        }
    }
}

// synchronize the content of `block` to disk.
// `ctx` can be NULL, which indicates this operation does not belong to any
// atomic operation and it immediately writes block content back to disk.
// However this is very dangerous, since it may break atomicity of
// concurrent atomic operations. YOU SHOULD USE THIS MODE WITH CARE. if
// `ctx` is not NULL, the actual writeback is delayed until `end_op`.
//
// NOTE: the caller must hold the lock of `block`.
// NOTE: if the number of blocks associated with `ctx` is larger than
// `OP_MAX_NUM_BLOCKS` after `sync`, `sync` should panic.

// hint:
// if ctx!=bull, just put the block on logheader, and update the usage of block in ctx.
// don't forget that you should keep this block in cache.
static void cache_sync(OpContext* ctx, Block* block) {
    if (ctx) {
        // TODO
        _acquire_spinlock(&log.lock);
        if ((int)header.num_blocks >= log.mx) {
            printk("too big a transaction\n");
            PANIC();
        }
        if (log.outstanding < 1) {
            printk("log_write outside of trans\n");
            PANIC();
        }
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
                printk("OP_MAX_BLOCK exceeded\n");
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
// end the atomic operation managed by `ctx`.
// it returns when all associated blocks are persisted to disk.
// hint:
// (move data from cache to log, then from log to disk and clean log) as known as 'commit' when all
// op are done.
static void cache_end_op(OpContext* ctx) {
    // TODO
    static int end;
    int do_commit = 0;
    _acquire_spinlock(&log.lock);
    // printk("END%d outstanding=%d\n", end++, log.outstanding);
    log.outstanding--;

    log.mu -= (int)ctx->rm;
    if (log.committing) {
        printk("log committing\n");
        PANIC();
    }
    if (log.outstanding == 0) {
        do_commit = 1, log.committing = 1;
    } else {

        int cnt = log.logcnt;
        log.logcnt = 0;
        // if (cnt) printk("Pa %d\n", cnt);
        for (int i = 0; i < cnt; i++)
            post_sem(&log.sem);
        wait_for_sem_locked(&log.sem);
        _release_spinlock(&log.lock);

        // FIXME
        // printk("wait ost\n");

        _acquire_spinlock(&log.lock);
        log.stcnt++;
        _release_spinlock(&log.lock);

        wait_sem(&log.outstandsem);

        _acquire_spinlock(&log.lock);
    }
    _release_spinlock(&log.lock);
    if (do_commit) {
        commit();
        _acquire_spinlock(&log.lock);
        // printk("com%d\n", log.stcnt);
        log.committing = 0;

        int cnt = log.stcnt;
        log.stcnt = 0;
        for (int i = 0; i < cnt; i++)
            post_sem(&log.outstandsem);
        wait_for_sem_locked(&log.outstandsem);

        cnt = log.logcnt;
        log.logcnt = 0;
        // if (cnt) printk("Pb %d\n", cnt);
        for (int i = 0; i < cnt; i++)
            post_sem(&log.sem);
        wait_for_sem_locked(&log.sem);
        // printk("post log\n");

        _release_spinlock(&log.lock);
    }
}

usize BBLOCK(usize b, const SuperBlock* sb) {
    return b / BIT_PER_BLOCK + sb->bitmap_start;
}
void bzero(OpContext* ctx, u32 block_no) {
    Block* bp = cache_acquire(block_no);
    memset(bp->data, 0, BLOCK_SIZE);
    cache_sync(ctx, bp);
    cache_release(bp);
}

// NOTES FOR BITMAP
//
// every block on disk has a bit in bitmap, including blocks inside bitmap!
//
// usually, MBR block, super block, inode blocks, log blocks and bitmap
// blocks are preallocated on disk, i.e. those bits for them are already set
// in bitmap. therefore when we allocate a new block, it usually returns a
// data block. however, nobody can prevent you freeing a non-data block :)

// allocate a new zero-initialized block, by searching bitmap for a free
// block. block number is returned.
//
// NOTE: if there's no free block on disk, `alloc` should panic.
// hint: you can use `cache_acquire`/`cache_sync` to read/write blocks.
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
    printk("cache_alloc: no free block\n");
    PANIC();
}

// mark block at `block_no` is free in bitmap.
// hint: you can use `cache_acquire`/`cache_sync` to read/write blocks.
static void cache_free(OpContext* ctx, usize block_no) {
    // TODO
    Block* bp;
    int bi, m;
    bp = cache_acquire(BBLOCK(block_no, sblock));
    bi = block_no % BIT_PER_BLOCK;
    m = 1 << (bi % 8);
    if ((bp->data[bi / 8] & m) == 0) {
        printk("freeing free block\n");
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
