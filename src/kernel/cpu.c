#include <kernel/cpu.h>
#include <kernel/printk.h>
#include <kernel/init.h>
#include <driver/clock.h>
#include <kernel/sched.h>
#include <aarch64/mmu.h>

struct cpu cpus[NCPU];

extern char exception_vector[];

static void cpu_clock_handler()
{
    struct proc* this = thisproc();
    if (this->ucontext->elr & KSPACE_MASK)
    {
        // ignore k-mode clock interrupts
        reset_clock(1000);
        printk("CPU %d: clock interrupt in kernel\n", cpuid());
    }
    else
    {
        // do context switch for user proc
        sched();
    }
}

static void init_clock_handler()
{
    set_clock_handler(&cpu_clock_handler);
}
early_init_func(init_clock_handler);

void set_cpu_on()
{
    ASSERT(!arch_disable_trap());
    arch_set_vbar(exception_vector);
    arch_reset_esr();
    init_clock();
    cpus[cpuid()].online = true;
    printk("CPU %d: hello\n", cpuid());
}

void set_cpu_off()
{
    arch_disable_trap();
    cpus[cpuid()].online = false;
    printk("CPU %d: stopped\n", cpuid());
}
