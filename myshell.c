#include <linux/limits.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include "LineParser.h"
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>



int main(){
    while(1){
        char cwd[PATH_MAX];
        char input[2048];
        getcwd(cwd, PATH_MAX);
        printf("%s\n", cwd);
        fgets(input, 2048, stdin); //hey bestie
        if(feof(stdin)){
            break;
        }
        cmdLine* cmdline= parseCmdLines(input);
        if(strcmp(cmdline->arguments[0], "cd") == 0){
            printf("Changing dir to %s\n", cmdline->arguments[1]);
            chdir(cmdline->arguments[1]);
            continue;
        }
        if(strcmp(cmdline->arguments[0], "zzzz") == 0){
                if(cmdline->argCount != 2){
                perror("zzzz not valid args count");
                _exit(1);
                }
                pid_t targetPid = atoi(cmdline->arguments[1]);
                kill(targetPid, SIGTSTP);
                continue;
    }

            if(strcmp(cmdline->arguments[0], "kuku") == 0){
                if(cmdline->argCount != 2){
                    perror("kuku not valid args count");
                    _exit(1);
                }
                pid_t targetPid = atoi(cmdline->arguments[1]);
                kill(targetPid, SIGCONT);
                continue;
            }

            if(strcmp(cmdline->arguments[0], "blast") == 0){
                if(cmdline->argCount != 2){
                    perror("blast not valid args count");
                    _exit(1);
                }
                pid_t targetPid = atoi(cmdline->arguments[1]);
                kill(targetPid, SIGINT);
                continue;
            }
        int pid=fork();
        if(cmdline->blocking >= 1){
            waitpid(pid, NULL, 0);
        }
         if(!pid){
            execute(cmdline);
        }
        freeCmdLines(cmdline);
    }
    return 0;

}

void execute(cmdLine* pCmdLine){
    if(strcmp(pCmdLine->arguments[0], "quit") == 0){
         _exit(0);
    }

    if (pCmdLine->inputRedirect != NULL) {
        int fd = open(pCmdLine->inputRedirect, O_RDONLY);
        if (fd < 0) {
            perror("open inputRedirect");
            _exit(1);
        }

        dup2(fd, STDIN_FILENO);   // 0
        close(fd);
    }

    if (pCmdLine->outputRedirect != NULL) {
        int fd = open(pCmdLine->outputRedirect, O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if (fd < 0) {
            perror("open outputRedirect");
            _exit(1);
        }
        dup2(fd, STDOUT_FILENO);  // 1
        close(fd);
    }

    int suc = execvp(pCmdLine->arguments[0], pCmdLine->arguments);
    
    if(suc == -1){
        perror("execvp failed");
        _exit(1);
    }
}