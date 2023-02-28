#include<stdlib.h>
#include<signal.h>
#include<unistd.h>
#include<stdio.h>

void sig_handler(int num){
    write(STDOUT_FILENO, "I won't die!.\n",13);
}

int main(){
    pid_t pid = fork();
    if(pid == 0){

    }
    else{
        signal(SIGCHLD,sig_handler);
        while(1){
        printf("Wasting your cycles. %d\n",getpid());
        sleep(1);
    }
    }
    
    while(1){
        printf("Wasting your cycles. %d\n",getpid());
        sleep(1);
    }
}