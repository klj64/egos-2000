/*
 * (C) 2022, Cornell University
 * All rights reserved.
 */

/* Author: Yunhao Zhang
 * Description: a simple shell
 */

#include "app.h"
#include "fs.h"
#include <string.h>

#define KGRN  "\x1B[1;32m"
#define KCYN  "\x1B[1;36m"
#define KNRM  "\x1B[1;0m"

int get_inode(int ino, char* name);
void parse_request(char* buf, struct proc_request* req);

int main() {
    SUCCESS("Enter kernel process GPID_SHELL");

    // get the inode of /home/yunhao
    int home = get_inode(0, "home");
    int yunhao = get_inode(home, "yunhao");
    char work_dir_name[DIR_NAME_SIZE], work_dir[256];
    strcpy(work_dir, "/home/yunhao");
    strcpy(work_dir_name, "yunhao");
    INFO("sys_shell: /home/yunhao has ino=%d", yunhao);

    
    /* Wait for shell commands */
    HIGHLIGHT("Welcome to egos-riscv!");
    char buf[128];
    while (1) {
        printf("%s➜ %s%s%s ", KGRN, KCYN, work_dir_name, KNRM);
        tty_read(buf, 100);
        if (strlen(buf) == 0) continue;

        if (strcmp(buf, "pwd") == 0) {
            printf("%s\r\n", work_dir);
        } else {
            int sender;
            struct proc_request req;
            struct proc_reply reply;
            parse_request(buf, &req);
            sys_send(GPID_PROCESS, (void*)&req, sizeof(req));
            sys_recv(&sender, (void*)&reply, sizeof(reply));

            if (reply.type != CMD_OK)
                INFO("sys_shell: command causes an error");
        }
    }
    return 0;
}

int get_inode(int ino, char* name) {
    int sender;
    struct dir_request req;
    char buf[SYSCALL_MSG_LEN];

    req.type = DIR_LOOKUP;
    req.ino = ino;
    strcpy(req.name, name);
    sys_send(GPID_DIR, (void*)&req, sizeof(struct dir_request));
    sys_recv(&sender, buf, SYSCALL_MSG_LEN);
    if (sender != GPID_DIR)
        FATAL("sys_shell expects message from GPID_DIR");

    struct dir_reply *reply = (void*)buf;
    return reply->ino;
}

void parse_request(char* buf, struct proc_request* req) {
    int len = strlen(buf);
    int idx = 0, nargs = 0;

    memset(req->argv, 0, CMD_NARGS * CMD_ARG_LEN);
    for (int i = 0; i < len; i++) {
        if (buf[i] != ' ') {
            req->argv[nargs][idx++] = buf[i];
        } else if (idx != 0) {
            nargs++;
            idx = 0;
        }
    }
    req->argc = ++nargs;
}