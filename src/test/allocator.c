#include <kernel/mem.h>
#include <kernel/printk.h>
#include <aarch64/intrinsic.h>
#include <common/rc.h>

extern RefCount alloc_page_cnt;

static volatile int x = 0;
static void* p[4][10000];

void alloc_test()
{
    int i = cpuid();
    int r = alloc_page_cnt.count;
    int y = 10000 - i * 500;
    if (i == 0)
        printk("alloc_page\n");
    x++;
    while (x < 4);
    arch_dsb_sy();
    for (int j = 0; j < y; j++)
        p[i][j] = kalloc_page();
    for (int j = 0; j < y; j++)
        kfree_page(p[i][j]);
    x++;
    while (x < 8);
    arch_dsb_sy();
    if (alloc_page_cnt.count != r)
        printk("FAIL: alloc_page_cnt %d -> %d\n", r, alloc_page_cnt);
    else
        x++;
    while (x < 12);
    arch_dsb_sy();
    for (int j = 0, t = i; j < y; j++)
        p[i][j] = kalloc(((t = t + (t >> 6) + (t >> 12) + j) & 1023) + 1);
    for (int j = 0; j < y; j++)
        kfree(p[i][j]);
    x++;
    while (x < 16);
    arch_dsb_sy();
    if (cpuid() == 0)
        printk("PASS\nUsage: %d\n", alloc_page_cnt.count - r);
}