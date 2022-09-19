
#include <driver/sddef.h>
/*
 * Initialize SD card and parse MBR.
 * 1. The first partition should be FAT and is used for booting.
 * 2. The second partition is used by our file system.
 *
 * See https://en.wikipedia.org/wiki/Master_boot_record
 */

// struct buf sdque;
SpinLock sdlock;
Queue sdque;
buf mbr;

u32 lba2, sz2;

define_init(sdcard) {
    sd_init();
    printk("sdinit ok");
}

void sd_init() {
    /*
     * Initialize the lock and request queue if any.
     * Remember to call sd_init() at somewhere.
     */
    /* TODO: Lab7 driver. */
    sdInit();
    init_spinlock(&sdlock);
    queue_init(&sdque);
    /*
     * Read and parse 1st block (MBR) and collect whatever
     * information you wan.
     *
     * Hint: Maybe need to use sd_start for reading, and
     * sdWaitForInterrupt for clearing certain interrupt.
     */
    mbr.blockno = 0;
    mbr.flags = 0;
    sd_start(&mbr);
    sdWaitForInterrupt(INT_READ_RDY);
    int done = 0;
    u32* ip = (u32*)mbr.data;
    while (done < 128)
        ip[done++] = *EMMC_DATA;
    sdWaitForInterrupt(INT_DATA_DONE);
    arch_dsb_sy();
    lba2 = *((u32*)(mbr.data + 470));
    sz2 = *((u32*)(mbr.data + 474));
    printk("lba2: %x\nsz2: %x\n", lba2, sz2);
    set_interrupt_handler(IRQ_SDIO, sd_intr);
    set_interrupt_handler(IRQ_ARASANSDIO, sd_intr);
    // printk("set IRQ_ARASANSDIO%d\n", IRQ_ARASANSDIO);
    /* TODO: Lab7 driver. */
}

/* Start the request for b. Caller must hold sdlock. */
static void sd_start(struct buf* b) {
    // Address is different depending on the card type.
    // HC pass address as block #.
    // SC pass address straight through.
    int bno = sdCard.type == SD_TYPE_2_HC ? (int)b->blockno : (int)b->blockno << 9;
    int write = b->flags & B_DIRTY;

    // printk("- sd start: cpu %d, flag 0x%x, bno %d, write=%d\n", cpuid(),
    // b->flags, bno, write);

    arch_dsb_sy();
    // Ensure that any data operation has completed before doing the transfer.
    if (*EMMC_INTERRUPT) {
        printk("emmc interrupt flag should be empty: 0x%x. \n", *EMMC_INTERRUPT);
        PANIC();
    }
    // asserts(!*EMMC_INTERRUPT, "emmc interrupt flag should be empty: 0x%x. ",
    //         *EMMC_INTERRUPT);
    arch_dsb_sy();

    // Work out the status, interrupt and command values for the transfer.
    int cmd = write ? IX_WRITE_SINGLE : IX_READ_SINGLE;

    int resp;
    *EMMC_BLKSIZECNT = 512;

    if ((resp = sdSendCommandA(cmd, bno))) {
        printk("* EMMC send command error.\n");
        PANIC();
    }

    int done = 0;
    u32* intbuf = (u32*)b->data;
    if (!(((i64)b->data) & 0x03) == 0) {
        printk("Only support word-aligned buffers. \n");
        PANIC();
    }
    // asserts((((i64)b->data) & 0x03) == 0,
    //         "Only support word-aligned buffers. ");

    if (write) {
        // Wait for ready interrupt for the next block.
        if ((resp = sdWaitForInterrupt(INT_WRITE_RDY))) {
            printk("* EMMC ERROR: Timeout waiting for ready to write\n");
            PANIC();
            // return sdDebugResponse(resp);
        }
        if (*EMMC_INTERRUPT) {
            printk("%d\n", *EMMC_INTERRUPT);
            PANIC();
        }
        // asserts(!*EMMC_INTERRUPT, "%d ", *EMMC_INTERRUPT);
        while (done < 128)
            *EMMC_DATA = intbuf[done++];
    }
}

/* The interrupt handler. */
void sd_intr() {
    /*
     * Pay attention to whether there is any element in the buflist.
     * Understand the meanings of EMMC_INTERRUPT, EMMC_DATA, INT_DATA_DONE,
     * INT_READ_RDY, B_DIRTY, B_VALID and some other flags.
     *
     * Notice that reading and writing are different, you can use flags
     * to identify.
     *
     * Remember to clear the flags after reading/writing.
     *
     * When finished, remember to use pop and check whether the list is
     * empty, if not, continue to read/write.
     *
     * You may use some buflist functions, arch_dsb_sy(), sd_start(), wakeup()
     * and sdWaitForInterrupt() to complete this function.
     */

    /* TODO: Lab7 driver. */
    queue_lock(&sdque);
    if (!queue_empty(&sdque)) {
        int intr = *EMMC_INTERRUPT;
        *EMMC_INTERRUPT = intr;
        buf* bid = container_of(queue_front(&sdque), buf, node);

        arch_dsb_sy();
        if (intr != INT_READ_RDY && intr != INT_DATA_DONE) {
            PANIC();
        }
        arch_dsb_sy();
        int is_write = (bid->flags & B_DIRTY);
        if ((is_write != 0 && intr != INT_DATA_DONE) || (is_write == 0 && intr != INT_READ_RDY)) {
            printk("%d | %d\n", is_write, intr);
            PANIC();
        }
        if (!is_write) {
            int done = 0;
            u32* ip = (u32*)bid->data;
            while (done < 128)
                ip[done++] = *EMMC_DATA;
            sdWaitForInterrupt(INT_DATA_DONE);
        }

        bid->flags = B_VALID;

        // wakeup(bid);
        post_sem(&bid->sl);
        queue_pop(&sdque);
        if (!queue_empty(&sdque)) {
            _acquire_spinlock(&sdlock);
            buf* buffer = container_of(queue_front(&sdque), buf, node);
            sd_start(buffer);
            _release_spinlock(&sdlock);
        }
        queue_unlock(&sdque);
    } else {
        queue_unlock(&sdque);
    }
}

/*
 * Sync buf with disk.
 * If B_DIRTY is set, write buf to disk, clear B_DIRTY, set B_VALID.
 * Else if B_VALID is not set, read buf from disk, set B_VALID.
 */
void sdrw(buf* b) {
    /*
     *
     * if list.size is 0, then use sd_start
     * else Add to the list
     *
     * then sleep, use loop to check whether buf flag is modified, if modified,
     * then break
     */

    /* TODO: Lab7 driver. */

    queue_lock(&sdque);

    if (queue_empty(&sdque)) {
        queue_push(&sdque, &b->node);
        _acquire_spinlock(&sdlock);

        buf* buffer = container_of(queue_front(&sdque), buf, node);
        sd_start(buffer);

        _release_spinlock(&sdlock);
    } else {
        queue_push(&sdque, &b->node);
    }
    init_sem(&b->sl, 0);
    while (true) {
        if (b->flags == B_VALID)
            break;

        // sleep(b, &qlock);
        queue_unlock(&sdque);
        wait_sem(&b->sl);
        queue_lock(&sdque);
    }

    queue_unlock(&sdque);
}

/* SD card test and benchmark. */
void sd_test() {
    static struct buf b[1 << 11];
    int n = sizeof(b) / sizeof(b[0]);
    int mb = (n * BSIZE) >> 20;
    // assert(mb);
    if (!mb)
        PANIC();
    i64 f, t;
    asm volatile("mrs %[freq], cntfrq_el0" : [freq] "=r"(f));
    printk("- sd test: begin nblocks %d\n", n);

    printk("- sd check rw...\n");
    // Read/write test
    for (int i = 1; i < n; i++) {
        // Backup.
        b[0].flags = 0;
        b[0].blockno = (u32)i;
        sdrw(&b[0]);
        // Write some value.
        b[i].flags = B_DIRTY;
        b[i].blockno = (u32)i;
        for (int j = 0; j < BSIZE; j++)
            b[i].data[j] = (u8)((i * j) & 0xFF);
        sdrw(&b[i]);

        memset(b[i].data, 0, sizeof(b[i].data));
        // Read back and check
        b[i].flags = 0;
        sdrw(&b[i]);
        for (int j = 0; j < BSIZE; j++) {
            //   assert(b[i].data[j] == (i * j & 0xFF));
            if (b[i].data[j] != (i * j & 0xFF))
                PANIC();
        }
        // Restore previous value.
        b[0].flags = B_DIRTY;
        sdrw(&b[0]);
    }

    // Read benchmark
    arch_dsb_sy();
    t = (i64)get_timestamp();
    arch_dsb_sy();
    for (int i = 0; i < n; i++) {
        b[i].flags = 0;
        b[i].blockno = (u32)i;
        sdrw(&b[i]);
    }
    arch_dsb_sy();
    t = (i64)get_timestamp() - t;
    arch_dsb_sy();
    printk("- read %dB (%dMB), t: %lld cycles, speed: %lld.%lld MB/s\n", n * BSIZE, mb, t,
           mb * f / t, (mb * f * 10 / t) % 10);

    // Write benchmark
    arch_dsb_sy();
    t = (i64)get_timestamp();
    arch_dsb_sy();
    for (int i = 0; i < n; i++) {
        b[i].flags = B_DIRTY;
        b[i].blockno = (u32)i;
        sdrw(&b[i]);
    }
    arch_dsb_sy();
    t = (i64)get_timestamp() - t;
    arch_dsb_sy();

    printk("- write %dB (%dMB), t: %lld cycles, speed: %lld.%lld MB/s\n", n * BSIZE, mb, t,
           mb * f / t, (mb * f * 10 / t) % 10);
}