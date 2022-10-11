#include <kernel/syscallno.h>

.global loop_start
.global loop_end

.align 12
loop_start:
    mov x4, x0
delay:
    mov x0, #10000
    mov x1, #0
    mov x2, #1
loop:
    add x1, x1, x2
    cmp x0, x1
    bne loop
    mov     x8, #SYS_myreport
    mov     x0, x4
    svc     #0
    b delay

.align 12
loop_end:
