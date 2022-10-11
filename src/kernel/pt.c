#include <kernel/pt.h>
#include <kernel/mem.h>
#include <common/string.h>
#include <aarch64/intrinsic.h>

PTEntriesPtr get_pte(struct pgdir* pgdir, u64 va, bool alloc)
{
    if (!pgdir->pt)
    {
        auto p = kalloc_page();
        memset(p, 0, PAGE_SIZE);
        pgdir->pt = p;
    }
    auto pt = pgdir->pt;
    int id[] = {VA_PART0(va), VA_PART1(va), VA_PART2(va), VA_PART3(va)};
    for (int i = 0; i < 3; i++)
    {
        if (!(pt[id[i]] & PTE_VALID))
        {
            if (!alloc)
                return NULL;
            auto p = kalloc_page();
            memset(p, 0, PAGE_SIZE);
            pt[id[i]] = K2P(p) + PTE_TABLE;
        }
        pt = (PTEntriesPtr)P2K(PTE_ADDRESS(pt[id[i]]));
    }
    return &pt[id[3]];
}

void init_pgdir(struct pgdir* pgdir)
{
    pgdir->pt = NULL;
}

static void _free_pt(PTEntriesPtr pt, int lv)
{
    for (int i = 0; i < N_PTE_PER_TABLE; i++)
    {
        if (pt[i] & PTE_VALID)
        {
            if (lv < 3)
                _free_pt((PTEntriesPtr)P2K(PTE_ADDRESS(pt[i])), lv + 1);
            else
                kfree_page((void*)P2K(PTE_ADDRESS(pt[i])));
        }
    }
    kfree_page(pt);
}

void free_pgdir(struct pgdir* pgdir)
{
    if (pgdir->pt)
        _free_pt(pgdir->pt, 0);
    pgdir->pt = NULL;
}

void attach_pgdir(struct pgdir* pgdir)
{
    extern PTEntries invalid_pt;
    if (pgdir->pt)
        arch_set_ttbr0(K2P(pgdir->pt));
    else
        arch_set_ttbr0(K2P(&invalid_pt));
}



