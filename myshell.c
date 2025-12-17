
#include <linux/limits.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include "LineParser.h"
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>

void execute(cmdLine* pCmdLine);
typedef struct process{
        cmdLine* cmd;                         /* the parsed command line*/
        pid_t pid; 		                  /* the process id that is running the command*/
        int status;                           /* status of the process: RUNNING/SUSPENDED/TERMINATED */
        struct process *next;	                  /* next process in chain */
    } process;
process* processes_list= NULL;
    #define TERMINATED  -1
    #define RUNNING 1
    #define SUSPENDED 0

    void addProcess(process** process_list, cmdLine* cmd, pid_t pid){
        process *new_process = (process *)malloc(sizeof(process));
        if (new_process == NULL) {
            perror("malloc failed");
            return;
        }
        new_process->cmd= cmd;
        new_process->pid=pid;
        new_process->status = RUNNING;
        new_process->next = *process_list;
        *process_list = new_process;
    }
void printProcessList(process **process_list) {
    process *curr = *process_list;
    while (curr != NULL) {
        printf("PID: %d | STATUS: %d | CMD: ", curr->pid, curr->status);
        if (curr->cmd != NULL) {
            for (int i = 0; i < curr->cmd->argCount; i++) {
                printf("%s ", curr->cmd->arguments[i]);
            }
        }
        printf("\n");
        curr = curr->next;
    }
}



int main() {
    while (1) {
        char cwd[PATH_MAX];
        char input[2048];

        getcwd(cwd, PATH_MAX);
        printf("%s\n", cwd);
        fgets(input, 2048, stdin);
        if(feof(stdin))

            break;

        cmdLine* cmdline = parseCmdLines(input);

        if (!cmdline || !cmdline->arguments[0]) {
            freeCmdLines(cmdline);
            continue;
        }

        /* built-in commands (run in shell) */
        if (strcmp(cmdline->arguments[0], "cd") == 0) {
            chdir(cmdline->arguments[1]);
            freeCmdLines(cmdline);
            continue;
        }
        if (strcmp(cmdline->arguments[0], "procs") == 0) {
            printProcessList(&processes_list);
            continue;
        }

        if (strcmp(cmdline->arguments[0], "zzzz") == 0) {
            kill(atoi(cmdline->arguments[1]), SIGTSTP);
            freeCmdLines(cmdline);
            continue;
        }

        if (strcmp(cmdline->arguments[0], "kuku") == 0) {
            kill(atoi(cmdline->arguments[1]), SIGCONT);
            freeCmdLines(cmdline);
            continue;
        }

        if (strcmp(cmdline->arguments[0], "blast") == 0) {
            kill(atoi(cmdline->arguments[1]), SIGINT);
            freeCmdLines(cmdline);
            continue;
        }

        execute(cmdline);
        freeCmdLines(cmdline);
    }
    return 0;
}
void execute(cmdLine* pCmdLine){

    /* ---------- NO PIPE ---------- */
    if (pCmdLine->next == NULL) {
        pid_t pid = fork();
        addProcess(&processes_list, pCmdLine, pid);
        if (pid == 0) {
            if (strcmp(pCmdLine->arguments[0], "quit") == 0)
                _exit(0);

            if (pCmdLine->inputRedirect) {
                int fd = open(pCmdLine->inputRedirect, O_RDONLY);
                dup2(fd, STDIN_FILENO);
                close(fd);
            }

            if (pCmdLine->outputRedirect) {
                int fd = open(pCmdLine->outputRedirect,
                              O_WRONLY | O_CREAT | O_TRUNC, 0666);
                dup2(fd, STDOUT_FILENO);
                close(fd);
            }

            execvp(pCmdLine->arguments[0], pCmdLine->arguments);
            perror("execvp");
            _exit(1);
        }

        if (pCmdLine->blocking)
            waitpid(pid, NULL, 0);

        return;
    }

    /* ---------- PIPE ---------- */
    if (pCmdLine->outputRedirect ||
        pCmdLine->next->inputRedirect) {
        fprintf(stderr, "Illegal redirection with pipe\n");
        return;
    }

    int fd[2];
    pipe(fd);

    pid_t pid1 = fork();
    addProcess(&processes_list, pCmdLine, pid1);
    if (pid1 == 0) {
        close(fd[0]);
        dup2(fd[1], STDOUT_FILENO);
        close(fd[1]);

        execvp(pCmdLine->arguments[0], pCmdLine->arguments);
        perror("execvp left");
        _exit(1);
    }

    pid_t pid2 = fork();
    addProcess(&processes_list, pCmdLine, pid2);
    if (pid2 == 0) {
        close(fd[1]);
        dup2(fd[0], STDIN_FILENO);
        close(fd[0]);

        execvp(pCmdLine->next->arguments[0],
               pCmdLine->next->arguments);
        perror("execvp right");
        _exit(1);
    }

    close(fd[0]);
    close(fd[1]);

    waitpid(pid1, NULL, 0);
    waitpid(pid2, NULL, 0);
}