#include <kernel/syscallno.h>
#include <kernel/proc.h>
#include <kernel/init.h>

#define NR_SYSCALL 512

extern void* syscall_table[NR_SYSCALL];

void syscall_entry(UserContext* context);

#define define_syscall(name, ...) \
void sys_##name__VA_ARGS__ \
static void __initsyscall_##name() { syscall_table[SYS_##name] = &sys_##name; } \
early_init_func(__initsyscall_##name) \
static sys_##name(__VA_ARGS__)
