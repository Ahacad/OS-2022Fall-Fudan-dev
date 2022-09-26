#include "common/ipc.h"
#include "test.h"
#include "kernel/printk.h"
#include "kernel/proc.h"
#include "kernel/mem.h"
struct mytype {
    int mtype;
    int sum;
};
static int msg[101];
static void sender(u64 start) {
    int msgid = sys_msgget(114514, 0);
    ASSERT(msgid >= 0);
    for (int i = start; i < (int)start + 10; i++) {
        struct mytype* k = (struct mytype*)kalloc(sizeof(struct mytype));
        k->mtype = i + 1;
        k->sum = -i + 1;
        sys_msgsnd(msgid, (msgbuf*)k, 4, 0);
    }
    exit(0);
}
static void receiver(u64 start) {
    int msgid = sys_msgget(114514, 0);
    ASSERT(msgid >= 0);
    for (int i = start; i < (int)start + 50; i++) {
        struct mytype k;
        ASSERT(sys_msgrcv(msgid, (msgbuf*)&k, 4, i + 1, 0) >= 0);
        msg[k.mtype] = k.sum;
    }
    exit(0);
}
void ipc_test() {
    printk("ipc_test\n");
    int key = 114514;
    int msgid;
    msgid = sys_msgget(key, IPC_CREATE | IPC_EXCL);
    for (int i = 0; i < 10; i++) {
        struct proc* p = kalloc(sizeof(struct proc));
        init_proc(p);
        start_proc(p, sender, i * 10);
    }
    for (int i = 0; i < 2; i++) {
        struct proc* p = kalloc(sizeof(struct proc));
        init_proc(p);
        start_proc(p, receiver, i * 50);
    }
    while (1) {
        int code;
        int id = wait(&code);
        if (id == -1)
            break;
    }
    ASSERT(sys_msgctl(msgid, IPC_RMID) >= 0);
    for (int i = 1; i < 101; i++)
        ASSERT(msg[i] = -i);
    printk("ipc_test PASS\n");
}