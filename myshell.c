
#include <limits.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include "LineParser.h"
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <ctype.h>  // for isdigit

#define TERMINATED  -1
#define RUNNING 1
#define SUSPENDED 0
#define HISTLEN 10

typedef struct process {
    cmdLine* cmd;          // the parsed command line
    pid_t pid;             // the process id that is running the command
    int status;            // status of the process: RUNNING/SUSPENDED/TERMINATED
    struct process *next;  // next process in chain
} process;

process* processes_list = NULL;
char* history[HISTLEN]; // circular queue
int history_start = 0;  // index of oldest command
int history_count = 0;  // number of commands stored

// Forward declaration
void execute(cmdLine* pCmdLine);

// -------------------- UTILITY --------------------
void normalizeCommand(char* cmd) {
    cmd[strcspn(cmd, "\n")] = 0; // remove newline
    size_t len = strlen(cmd);
    if (len > 0 && cmd[len-1] == '&') cmd[len-1] = 0; // remove trailing &
}

// -------------------- HISTORY FUNCTIONS --------------------
void addHistory(const char* cmdline) {
    char* copy = strdup(cmdline);
    if (!copy) {
        perror("strdup failed");
        return;
    }
    if (history_count < HISTLEN) {
        int index = (history_start + history_count) % HISTLEN;
        history[index] = copy;
        history_count++;
    } else {
        free(history[history_start]);
        history[history_start] = copy;
        history_start = (history_start + 1) % HISTLEN;
    }
}

void printHistory() {
    for (int i = 0; i < history_count; i++) {
        int index = (history_start + i) % HISTLEN;
        printf("%d: %s\n", i, history[index]);
    }
}

void executeCommandFromHistory(char* cmdline) {
    char cmdline_copy[2048];
    strncpy(cmdline_copy, cmdline, sizeof(cmdline_copy));
    cmdline_copy[sizeof(cmdline_copy)-1] = 0;
    normalizeCommand(cmdline_copy); // normalize before execution

    addHistory(cmdline_copy); // store it again
    cmdLine* parsed = parseCmdLines(cmdline_copy);
    execute(parsed);
    freeCmdLines(parsed);
}

void executeLastCommand() {
    if (history_count == 0) {
        printf("No commands in history.\n");
        return;
    }
    int last_index = (history_start + history_count - 1) % HISTLEN;
    executeCommandFromHistory(history[last_index]);
}

void executeHistoryIndex(int n) {
    if (n < 0 || n >= history_count) {
        printf("No such command in history.\n");
        return;
    }
    int index = (history_start + n) % HISTLEN;
    executeCommandFromHistory(history[index]);
}

// -------------------- PROCESS LIST FUNCTIONS --------------------
char* statusToString(int status) {
    switch(status) {
        case RUNNING: return "RUNNING";
        case SUSPENDED: return "SUSPENDED";
        case TERMINATED: return "TERMINATED";
        default: return "UNKNOWN";
    }
}

void updateProcessStatus(process *process_list, int pid, int status) {
    while(process_list) {
        if(process_list->pid == pid) {
            process_list->status = status;
            return;
        }
        process_list = process_list->next;
    }
}

void freeProcessList(process *process_list) {
    process *curr = process_list;
    while(curr) {
        process *next = curr->next;
        if(curr->cmd) freeCmdLines(curr->cmd);
        free(curr);
        curr = next;
    }
}

void updateProcessList(process **process_list) {
    process *curr = *process_list;
    int status;
    pid_t result;

    while(curr) {
        result = waitpid(curr->pid, &status, WNOHANG | WUNTRACED | WCONTINUED);
        if(result > 0) {
            if(WIFEXITED(status) || WIFSIGNALED(status))
                updateProcessStatus(*process_list, curr->pid, TERMINATED);
            else if(WIFSTOPPED(status))
                updateProcessStatus(*process_list, curr->pid, SUSPENDED);
            else if(WIFCONTINUED(status))
                updateProcessStatus(*process_list, curr->pid, RUNNING);
        }
        curr = curr->next;
    }
}

void printProcessList(process **process_list) {
    updateProcessList(process_list);
    process *curr = *process_list;
    process *prev = NULL;

    printf("PID\t\tCommand\t\tSTATUS\n");
    while(curr) {
        printf("%d\t\t", curr->pid);
        for(int i=0; i<curr->cmd->argCount; i++)
            printf("%s ", curr->cmd->arguments[i]);
        printf("\t%s\n", statusToString(curr->status));

        if(curr->status == TERMINATED) {
            process *to_delete = curr;
            if(prev == NULL) *process_list = curr->next;
            else prev->next = curr->next;
            curr = curr->next;
            freeCmdLines(to_delete->cmd);
            free(to_delete);
        } else {
            prev = curr;
            curr = curr->next;
        }
    }
}

void addProcess(process **process_list, cmdLine *cmd, pid_t pid) {
    process *new_process = malloc(sizeof(process));
    if(!new_process) {
        perror("malloc failed");
        return;
    }
    new_process->cmd = cmd;
    new_process->pid = pid;
    new_process->status = RUNNING;
    new_process->next = *process_list;
    *process_list = new_process;
}

// -------------------- EXECUTE FUNCTION --------------------
void execute(cmdLine* pCmdLine) {
    if(!pCmdLine) return;

    if(pCmdLine->next == NULL) { // NO PIPE
        pid_t pid = fork();
        addProcess(&processes_list, pCmdLine, pid);
        if(pid == 0) {
            if(pCmdLine->inputRedirect) {
                int fd = open(pCmdLine->inputRedirect, O_RDONLY);
                dup2(fd, STDIN_FILENO);
                close(fd);
            }
            if(pCmdLine->outputRedirect) {
                int fd = open(pCmdLine->outputRedirect,
                              O_WRONLY | O_CREAT | O_TRUNC, 0666);
                dup2(fd, STDOUT_FILENO);
                close(fd);
            }
            execvp(pCmdLine->arguments[0], pCmdLine->arguments);
            perror("execvp");
            _exit(1);
        }
        if(pCmdLine->blocking)
            waitpid(pid, NULL, 0);
        return;
    }

    // PIPE
    if(pCmdLine->outputRedirect || pCmdLine->next->inputRedirect) {
        fprintf(stderr,"Illegal redirection with pipe\n");
        return;
    }

    int fd[2]; pipe(fd);
    pid_t pid1 = fork();
    addProcess(&processes_list, pCmdLine, pid1);
    if(pid1==0) {
        close(fd[0]);
        dup2(fd[1], STDOUT_FILENO);
        close(fd[1]);
        execvp(pCmdLine->arguments[0], pCmdLine->arguments);
        perror("execvp left");
        _exit(1);
    }

    pid_t pid2 = fork();
    addProcess(&processes_list, pCmdLine, pid2);
    if(pid2==0) {
        close(fd[1]);
        dup2(fd[0], STDIN_FILENO);
        close(fd[0]);
        execvp(pCmdLine->next->arguments[0], pCmdLine->next->arguments);
        perror("execvp right");
        _exit(1);
    }

    close(fd[0]); close(fd[1]);
    waitpid(pid1, NULL, 0);
    waitpid(pid2, NULL, 0);
}

// -------------------- MAIN --------------------
int main() {
    while (1) {
        char cwd[PATH_MAX];
        char input[2048];

        // print cwd
        if (!getcwd(cwd, PATH_MAX)) perror("getcwd");
        printf("%s\n", cwd);

        // read input
        if (!fgets(input, sizeof(input), stdin)) break;
        input[strcspn(input, "\n")] = 0; // remove newline

        char cmd_buffer[2048];  // command to execute
        cmdLine* cmdline = NULL;

        // handle !! and !n
        if (strcmp(input, "!!") == 0) {
            if (history_count == 0) {
                printf("No commands in history.\n");
                continue;
            }
            int last_index = (history_start + history_count - 1) % HISTLEN;
            strncpy(cmd_buffer, history[last_index], sizeof(cmd_buffer));
            cmd_buffer[sizeof(cmd_buffer) - 1] = 0;
            printf("%s\n", cmd_buffer);
        }
        else if (input[0] == '!' && isdigit(input[1])) {
            int n = atoi(input + 1);
            if (n < 0 || n >= history_count) {
                printf("No such command in history.\n");
                continue;
            }
            int hist_index = (history_start + n) % HISTLEN;
            strncpy(cmd_buffer, history[hist_index], sizeof(cmd_buffer));
            cmd_buffer[sizeof(cmd_buffer) - 1] = 0;
            printf("%s\n", cmd_buffer);
            
        }
        else {
            strncpy(cmd_buffer, input, sizeof(cmd_buffer));
            cmd_buffer[sizeof(cmd_buffer) - 1] = 0;
        }

        // add to history (except empty)
        if (strlen(cmd_buffer) > 0)
            addHistory(cmd_buffer);

        cmdline = parseCmdLines(cmd_buffer);
        if (!cmdline || !cmdline->arguments[0]) {
            freeCmdLines(cmdline);
            continue;
        }

        // built-in commands
        if (strcmp(cmdline->arguments[0], "cd") == 0) {
            chdir(cmdline->arguments[1]);
            freeCmdLines(cmdline);
            continue;
        }
        if (strcmp(cmdline->arguments[0], "procs") == 0) {
            printProcessList(&processes_list);
            freeCmdLines(cmdline);
            continue;
        }
        if (strcmp(cmdline->arguments[0], "history") == 0) {
            printHistory();
            freeCmdLines(cmdline);
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