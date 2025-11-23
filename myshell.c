#include <linux/limits.h>
#include <stdio.h>
#include <unistd.h>
#include "LineParser.h"


int main(){
    while(1){
        char cwd[PATH_MAX];
        char input[2048];
        getcwd(cwd, PATH_MAX);
        printf("%s\n", cwd);
        fgets(input, 2048, stdin);
        cmdLine* cmdline= parseCmdLines(input);
        printf("%s\n", cmdline->arguments[0]); 
        excecute(cmdline);
        freeCmdLines(cmdline);
    }
    return 0;

}

void excecute(cmdLine* pCmdLine){
     if(pCmdLine->arguments[0] == "quit"){
         _exit(0);
}    
    int suc =execvp(pCmdLine->arguments[0], pCmdLine->arguments);
    if(suc == -1){
        perror("execv failed");
        _exit(1);
    }
}