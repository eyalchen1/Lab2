#include <stdio.h>      // for printf
#include <unistd.h>     // for pipe, fork, read, write, close
#include <string.h>     // for strlen
#include <sys/wait.h>   // for wait, NULL
#include <stdlib.h>     // optional: for exit()
int main(int argc, char *argv[]){
    int fd[2];
    char buffer[2048];
    if(pipe(fd) == -1){
        printf("error");
        return 1;
    }
    int pid= fork();
    if(!pid){
        close(fd[0]);
        write(fd[1], argv[1], +strlen(argv[1])+1);
        close(fd[1]);
    }
    else{
        waitpid(pid, NULL, 0);
        close(fd[1]);
        read(fd[0], buffer, sizeof(buffer));
        printf("%s\n", buffer);
        close(fd[0]);
    }
}