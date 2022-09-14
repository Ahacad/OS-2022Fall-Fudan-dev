#include <kernel/mem.h>
#include <kernel/printk.h>
#include <aarch64/intrinsic.h>
#include <common/rc.h>
#include <test/test.h>

extern RefCount alloc_page_cnt;

RefCount x;
static void* p[4][10000];

void alloc_test()
{
    int i = cpuid();
    int r = alloc_page_cnt.count;
    int y = 10000 - i * 500;
    if (i == 0)
        printk("alloc_page\n");
    _increment_rc(&x);
    while (x.count < 4);
    arch_dsb_sy();
    for (int j = 0; j < y; j++)
        p[i][j] = kalloc_page();
    for (int j = 0; j < y; j++)
        kfree_page(p[i][j]);
    _increment_rc(&x);
    while (x.count < 8);
    arch_dsb_sy();
    if (alloc_page_cnt.count != r)
        printk("FAIL: alloc_page_cnt %d -> %d\n", r, alloc_page_cnt);
    else
        _increment_rc(&x);
    while (x.count < 12);
    arch_dsb_sy();
    for (int j = 0; j < 10000; )
    {
        
        if (j < 1000 || rand() > RAND_MAX / 3)
        {
            int z = (rand() & 1023) + 1;
            p[i][j] = kalloc(z);
            if (p[i][j] == NULL)
            {
                printk("FAIL: alloc(%d) = null\n", z);
                while (1);
            }
            *(char*)p[i][j++] = i;
        }
        else
        {
            int k = rand() % j;
            if (p[i][k] == NULL || *(char*)p[i][k] != i)
            {
                printk("FAIL: p[%d][%d] %d wrong\n", i, k,*(char*)p[i][k]);
                while (1);
            }
            kfree(p[i][k]);
            p[i][k] = p[i][--j];
        }
    }
    for (int j = 0; j < 10000; j++)
        kfree(p[i][j]);
    _increment_rc(&x);
    while (x.count < 16);
    arch_dsb_sy();
    if (cpuid() == 0)
        printk("PASS\nUsage: %d\n", alloc_page_cnt.count - r);
}