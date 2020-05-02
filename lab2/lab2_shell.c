#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <linux/sched.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>

#define MAX_CMDLINE_LENGTH 256  /*max cmdline length in a line*/
#define MAX_CMD_LENGTH 16       /*max single cmdline length*/
#define MAX_CMD_NUM 16          /*max single cmdline length*/

#ifndef NR_TASKS                /*max task num*/
#define NR_TASKS 64
#endif

#define SHELL "/bin/sh"

static int *child_pid = NULL;   /*save running children's pid*/

/* popen，输入为命令和类型("r""w")，输出执行命令进程的I/O文件描述符 */
int os_popen(const char* cmd, const char type){
    int         i, pipe_fd[2], proc_fd;  /* pipe_fd[0]为读端, pipe_fd[1]为写端 */
    pid_t       pid;  

    if (type != 'r' && type != 'w') {
        printf("popen() flag error\n");
        return 0;  
    }  

    if(child_pid == NULL) {
        if ((child_pid = calloc(NR_TASKS, sizeof(int))) == NULL)
            return 0;
    }

    if (pipe(pipe_fd) < 0) {
        printf("popen() pipe create error\n");
        return 0; 
    }

    /* 1. 使用系统调用创建新进程 */
    pid = fork();

    /* 2. 子进程部分 */
    if(pid == 0) {                             
        if (type == 'r') {  /* parent process read */
            /* 2.1 关闭pipe无用的一端，将I/O输出发送到父进程 */
            close(pipe_fd[0]);  
            if (pipe_fd[1] != STDOUT_FILENO) {  
                dup2(pipe_fd[1], STDOUT_FILENO);  
                close(pipe_fd[1]);  		/* 关闭多余的写端 */
            }  
        }
        else {  /* parent process write */
            /* 2.2 关闭pipe无用的一端，接收父进程提供的I/O输入 */
            close(pipe_fd[1]);  
            if (pipe_fd[0] != STDIN_FILENO) {  
                dup2(pipe_fd[0], STDIN_FILENO);  
                close(pipe_fd[0]);  
            }  
        }  
        /* 关闭所有未关闭的子进程文件描述符（无需修改） */
        for (i = 0; i < NR_TASKS; i++)
            if(child_pid[i] > 0)
                close(i);
        /* 2.3 通过execl系统调用运行命令 */
        execl(SHELL, "sh", "-c", cmd, NULL);
        _exit(127);  
    }  

    /* 3. 父进程部分 */                              
    if (type == 'r') {  /* read */
       	close(pipe_fd[1]);  
       	proc_fd = pipe_fd[0];
    }
    else {  			/* write */
        close(pipe_fd[0]);  
        proc_fd = pipe_fd[1]; 
    }    
    child_pid[proc_fd] = pid;
    return proc_fd;
}

/* 关闭正在打开的管道，等待对应子进程运行结束（无需修改） */
int os_pclose(const int fno) {
    int stat;
    pid_t pid;
    if (child_pid == NULL)
        return -1;
    if ((pid = child_pid[fno]) == 0)
        return -1;
    child_pid[fno] = 0;
    close(fno);
    while (waitpid(pid, &stat, 0) < 0)
        if(errno != EINTR)
            return -1;
    return stat;
}

int os_system(const char* cmdstring) {
    pid_t pid;
    int stat;
    int cmd_num;

    if(cmdstring == NULL) {
        printf("nothing to do\n");
        return 1;
    }

    /* 4.1 创建一个新进程 */
    pid = fork();

    /* 4.2 子进程部分 */
    if(pid == 0) {
        execl(SHELL, "sh", "-c", cmdstring, NULL);
	_exit(127);
    }

    /* 4.3 父进程部分: 等待子进程运行结束 */
    else if (pid > 0) {
        waitpid(pid, &stat, 0);
    }

    return stat;
}

/* 对cmdline按照";"做划分，返回划分段数 */

int parseCmd(char* cmdline, char cmds[MAX_CMD_NUM][MAX_CMD_LENGTH]) {
    int i,j;
    int offset = 0;
    int cmd_num = 0;
    char tmp[MAX_CMD_LENGTH];
    int len = 0;
    char *end = strchr(cmdline, ';');
    char *start = cmdline;
    while (end != NULL) {
        memcpy(cmds[cmd_num], start, end - start);
        cmds[cmd_num++][end - start] = '\0';
        start = end + 1;
        end = strchr(start, ';');
    };
    len = strlen(cmdline);
    if (start < cmdline + len) {
        memcpy(cmds[cmd_num], start, (cmdline + len) - start);
        cmds[cmd_num++][(cmdline + len) - start] = '\0';
    }
    return cmd_num;
}

void zeroBuff(char* buff, int size) {
    int i;
    for(i = 0; i < size; i++){
        buff[i] = '\0';
    }
}

int main(void) {
    int     cmd_num, i, j, fd1, fd2, count, status;
    pid_t   pids[MAX_CMD_NUM];
    char    buf[4096];
    while(1){
        /* 将标准输出文件描述符作为参数传入write，即可实现print */
    	char    cmdline[MAX_CMDLINE_LENGTH];
    	char    cmds[MAX_CMD_NUM][MAX_CMD_LENGTH];
		write(STDOUT_FILENO, "os shell ->", 11);
        read(STDIN_FILENO, cmdline, MAX_CMDLINE_LENGTH);
        cmd_num = parseCmd(cmdline, cmds);
        for(i = 0; i < cmd_num; i++) {
            char *div = strchr(cmds[i], '|');
            if (div) {
                /* 如果需要用到管道功能 */
                char cmd1[MAX_CMD_LENGTH], cmd2[MAX_CMD_LENGTH];
                int len = div - cmds[i];
                memcpy(cmd1, cmds[i], len);	/* 获取管道符'|'之前的命令 */
                cmd1[len] = '\0';
                len = (cmds[i] + strlen(cmds[i])) - div - 1;
                memcpy(cmd2, div + 1, len);	/* 获取管道符'|'之后的命令 */
                cmd2[len] = '\0';
                printf("cmd1: %s\n", cmd1);
                printf("cmd2: %s\n", cmd2);
                /* 5.1 运行cmd1，并将cmd1标准输出存入buf中 */
				zeroBuff(buf, 4096);
				fd1 = os_popen(cmd1, 'r');
				count = read(fd1, buf, 4096);
				status = os_pclose(fd1);

                /* 5.2 运行cmd2，并将buf内容写入到cmd2输入中 */
                fd2 = os_popen(cmd2, 'w');
				write(fd2, buf, count);
				status = os_pclose(fd2);
				zeroBuff(buf, 4096);
            }
            else {
                /* 6 一般命令的运行 */
				os_system(cmds[i]);
            }
	    	zeroBuff(cmds[i], MAX_CMDLINE_LENGTH);
        }
    }
    return 0;

}




