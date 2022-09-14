#include <kernel/mem.h>
#include <kernel/printk.h>
#include <aarch64/intrinsic.h>
#include <common/rc.h>
#include <test/test.h>

extern RefCount alloc_page_cnt;

RefCount x;
static void* p[4][10000];

#define FAIL(...) {printk(__VA_ARGS__); while (1);}
#define SYNC(i) _increment_rc(&x); while (x.count < 4 * i); arch_dsb_sy();

void alloc_test()
{
    int i = cpuid();
    int r = alloc_page_cnt.count;
    int y = 10000 - i * 500;
    if (i == 0)
        printk("alloc_test\n");
    SYNC(1)
    for (int j = 0; j < y; j++)
    {
        p[i][j] = kalloc_page();
        if ((u64)p[i][j] & 1023)
            FAIL("FAIL: alloc_page() = %p\n", p[i][j]);
    }
    for (int j = 0; j < y; j++)
        kfree_page(p[i][j]);
    SYNC(2)
    if (alloc_page_cnt.count != r)
        FAIL("FAIL: alloc_page_cnt %d -> %d\n", r, alloc_page_cnt);
    SYNC(3)
    for (int j = 0; j < 10000; )
    {
        if (j < 1000 || rand() > RAND_MAX / 5 * 2)
        {
            int z = 0;
            switch (rand() & 3)
            {
                case 0: z = rand() & 31; break;
                case 1: z = rand() & 126; break;
                case 2: z = rand() & 508; break;
                case 3: z = rand() & 2040; break;
            }
            z += !z;
            p[i][j] = kalloc(z);
            u64 q = (u64)p[i][j];
            if (p[i][j] == NULL ||
                ((z & 1) == 0 && (q & 1) != 0) ||
                ((z & 3) == 0 && (q & 3) != 0) ||
                ((z & 7) == 0 && (q & 7) != 0))
                FAIL("FAIL: alloc(%d) = %p\n", z, p[i][j]);
            *(char*)p[i][j++] = i;
        }
        else
        {
            int k = rand() % j;
            if (p[i][k] == NULL || *(char*)p[i][k] != i)
                FAIL("FAIL: p[%d][%d] wrong\n", i, k);
            kfree(p[i][k]);
            p[i][k] = p[i][--j];
        }
    }
    SYNC(4)
    if (cpuid() == 0)
        printk("Usage: %d\n", alloc_page_cnt.count - r);
    SYNC(5)
    for (int j = 0; j < 10000; j++)
        kfree(p[i][j]);
    SYNC(6)
    if (cpuid() == 0)
        printk("alloc_test PASS\n");
    // theoretically best: ~3300
}
