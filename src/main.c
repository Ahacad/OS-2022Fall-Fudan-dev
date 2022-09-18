#include <common/spinlock.h>
#include <common/string.h>
#include <kernel/mem.h>
#include <kernel/init.h>
#include <kernel/printk.h>
#include <kernel/sched.h>
#include <driver/sd.h>

static bool boot_secondary_cpus = false;

NO_RETURN void idle_entry();

void kernel_init() {
    // clear BSS section.
    extern char edata[], end[];
    memset(edata, 0, (usize)(end - edata));

    do_early_init();
    do_init();

    boot_secondary_cpus = true;
}

void test1(int x) {
    setup_checker(111);
    checker_begin_ctx(111);
    if (x == 0) {
        checker_end_ctx(111);
        return;
    }
    checker_end_ctx(111);
}

void main() {
    if (cpuid() == 0) {
        kernel_init();
    } else {
        while (!boot_secondary_cpus)
            ;
        arch_dsb_sy();
    }

    // enter idle process
    set_return_addr(idle_entry);
}
