/*
 * (C) 2022, Cornell University
 * All rights reserved.
 */

/* Author: Yunhao Zhang
 * Description: helper functions for managing processes
 */

#include "egos.h"
#include "process.h"
#include "syscall.h"
#include <string.h>

#define INTR_ID_SOFT       3
void intr_entry(int id);

void excp_entry(int id) {
    /* Student's code goes here: */

    /* If the exception is a system call, handle the system call and return */
    if(id == 8 || id == 11){
        int mepc;
        asm("csrr %0, mepc" : "=r"(mepc));
        asm("csrw mepc, %0" ::"r"(mepc + 4));
        intr_entry(INTR_ID_SOFT);
        return;
    }
    /* Kill the process if curr_pid is a user app instead of a grass server */

    if (curr_pid >= GPID_USER_START) { // what is earth->tty_intr()?
        /* User process killed by ctrl+c interrupt */
        INFO("process %d terminated with exception %d", curr_pid, id);
        asm("csrw mepc, %0" ::"r"(0x8005008));
        return;
    }
    /* Student's code ends here. */

    FATAL("excp_entry: kernel got exception %d", id);
}

void proc_init()
{
    earth->intr_register(intr_entry);
    earth->excp_register(excp_entry);

    /* Student's code goes here: */
    // 0b0/00/A/xwr
    /* Setup PMP TOR region 0x00000000 - 0x08008000 as r/w/x */
    // 0b00001111
    int cfg;
    asm("csrw pmpaddr0, %0" ::"r"(0x08008000));
    cfg = 0x0F;

    /* Setup PMP NAPOT region 0x20400000 - 0x20800000 as r/-/x */
    // 0b00011101
    // We need to always rightshift by 2 to change the 34 byte address to 32 byte format. 
    // Then we need to clear the bits of our address by right and then left shifting. 
    // In our case we have 2^22 range so we clear n-2 bits. 22-2 = 20 bits.
    // then we add one 0 followed by nineteen 1's to the end.
    // where hex((1<<22)-1) = 0x3fffff
    asm("csrw pmpaddr1, %0" ::"r"((((0x20400000 >> 2) >> 20) << 20) + 0x3ffff));
    cfg = cfg | (0b00011101 << 8);

    /* Setup PMP NAPOT region 0x20800000 - 0x20C00000 as r/-/- */
    // where
    asm("csrw pmpaddr2, %0" ::"r"((((0x20800000 >> 2) >> 20) << 20) + 0x3ffff));
    cfg = cfg | (0b00011101 << 16);
    /* Setup PMP NAPOT region 0x80000000 - 0x80004000 as r/w/- */
    //>>> hex((1<<15)-1)
    // 2^14 is address size of our range. Thus: n-2 = 14-2=12
    // 0x7ff is one 0 followed by eleven 1's.
    asm("csrw pmpaddr3, %0" ::"r"((((0x80000000 >> 2) >> 12) << 12) + 0x7ff));
    cfg = cfg | (0b00011101 << 24);
    asm("csrw pmpcfg0, %0" ::"r"(cfg));
    /* Student's code ends here. */

    /* The first process is currently running */
    proc_set_running(proc_alloc());
}

static void proc_set_status(int pid, int status) {
    for (int i = 0; i < MAX_NPROCESS; i++)
        if (proc_set[i].pid == pid) proc_set[i].status = status;
}

int proc_alloc() {
    static int proc_nprocs = 0;
    for (int i = 0; i < MAX_NPROCESS; i++)
        if (proc_set[i].status == PROC_UNUSED) {
            proc_set[i].pid = ++proc_nprocs;
            proc_set[i].status = PROC_LOADING;
            return proc_nprocs;
        }

    FATAL("proc_alloc: reach the limit of %d processes", MAX_NPROCESS);
}

void proc_free(int pid) {
    if (pid != -1) {
        earth->mmu_free(pid);
        proc_set_status(pid, PROC_UNUSED);
    } else {
        /* Free all user applications */
        for (int i = 0; i < MAX_NPROCESS; i++)
            if (proc_set[i].pid >= GPID_USER_START &&
                proc_set[i].status != PROC_UNUSED) {
                earth->mmu_free(proc_set[i].pid);
                proc_set[i].status = PROC_UNUSED;
            }
    }
}

void proc_set_ready(int pid) { proc_set_status(pid, PROC_READY); }
void proc_set_running(int pid) { proc_set_status(pid, PROC_RUNNING); }
void proc_set_runnable(int pid) { proc_set_status(pid, PROC_RUNNABLE); }
