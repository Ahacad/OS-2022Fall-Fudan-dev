#include <aarch64/trap.h>
#include <aarch64/intrinsic.h>
#include <kernel/sched.h>
#include <kernel/printk.h>
#include <driver/interrupt.h>
#include <kernel/proc.h>

void trap_global_handler(UserContext* context)
{
    thisproc()->ucontext = context;

    u64 esr = arch_get_esr();
    u64 ec = esr >> ESR_EC_SHIFT;
    u64 iss = esr & ESR_ISS_MASK;
    u64 ir = esr & ESR_IR_MASK;

    arch_reset_esr();

    switch (ec)
    {
        case ESR_EC_UNKNOWN:
        {
            if (ir)
                PANIC();
            else
                interrupt_global_handler();
        } break;
        case ESR_EC_SVC64:
        {
            // TODO: syscall
            PANIC();
        } break;
        default:
        {
            // TODO: other exception
            PANIC();
        }
    }
}

NO_RETURN void trap_error_handler(u64 type)
{
    printk("Unknown trap type %d\n", type);
    PANIC();
}
