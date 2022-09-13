#include <kernel/cpu.h>
#include <kernel/printk.h>
#include <kernel/init.h>
#include <driver/clock.h>
#include <kernel/sched.h>
#include <kernel/proc.h>
#include <aarch64/mmu.h>

struct cpu cpus[NCPU];

extern char exception_vector[];

static void cpu_clock_handler()
{
    auto this = thisproc();
    if (this->ucontext->elr & KSPACE_MASK)
    {
        // ignore k-mode clock interrupts
        printk("CPU %d: clock interrupt in kernel\n", cpuid());
        reset_clock(1000);
    }
    else
    {
        // do context switch for user proc
        yield();
    }
}

define_early_init(clock_handler)
{
    set_clock_handler(&cpu_clock_handler);
}

void set_cpu_on()
{
    ASSERT(!_arch_disable_trap());
    arch_set_vbar(exception_vector);
    arch_reset_esr();
    init_clock();
    cpus[cpuid()].online = true;
    printk("CPU %d: hello\n", cpuid());
}

void set_cpu_off()
{
    _arch_disable_trap();
    cpus[cpuid()].online = false;
    printk("CPU %d: stopped\n", cpuid());
}
