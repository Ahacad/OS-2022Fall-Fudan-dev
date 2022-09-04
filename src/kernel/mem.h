#pragma once

#include <common/defines.h>
#include <aarch64/mmu.h>

#define PPN_MAX (0x3F000000/4096)

void* kalloc_page();
void kfree_page(void*);

void* kalloc(isize);
void kfree(void*);