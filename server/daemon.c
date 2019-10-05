#include "daemon.h"

#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>

void child_handler()
{
    pid_t pid = 1;

    while(pid>0) // Claim Zombies
    {
        pid = waitpid(0, (int*)0, WNOHANG);
    }
}

void daemon_init()
{
    pid_t pid;
    struct sigaction sig_act;

    if((pid = fork()) < 0)
    {
        perror("fork");
        exit(1);
    } else if(pid > 0)
    {
        printf("Daemon PID: %d\n", (int)pid);
        exit(0);    /* Bye-bye parent */
    }



    setsid();       /* Become session leader */
    umask(0);       /* Clear file mode creation mask */

    /* Catch SIGCHLD to remove zombies from system */
    sig_act.sa_handler = child_handler;
    sigemptyset(&sig_act.sa_mask);
    sig_act.sa_flags = SA_NOCLDSTOP;
    sigaction(SIGCHLD, (struct sigaction*) &sig_act, (struct sigaction*) 0);
}