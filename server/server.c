
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <dirent.h>

// My headers
#include "daemon.h"
#include "handlers.h"
#include "stream.h"

#define SERV_TCP_PORT 40005

void server_a_client(int sock_d);

int main(int argc, char** argv)
{
    int sock_d;                     // Socket descriptor
    int acc_sd;                     // File descriptor for the accepted socket
    struct sockaddr_in serv_addr;   // Server socket address
    struct sockaddr_in cli_addr;    // Client socket address
    socklen_t cli_addrlen;          // Length of the client address
    pid_t pid;                      // pid for child processes


    /* Check the command line args */
    if(argc == 1 || argc > 2)
    {
        printf("Usage: %s [ initial_current_directory ]\n", argv[0]);
        printf("E.g. %s /\n", argv[0]);
        exit(1);
    }
    else if(argc == 2)
    {
        if(chdir(argv[1]) == -1){
            printf("Initial current directory not found!\n");
            exit(1);
        }
    }

    /* Turn server into a daemon */
    daemon_init();

    if((sock_d = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        perror("server:socket");
        exit(1);
    }

    bzero((char*) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family        = AF_INET;
    serv_addr.sin_port          = htons(SERV_TCP_PORT);
    serv_addr.sin_addr.s_addr   = htonl(INADDR_ANY);

    /* bind server address to socket */
    if(bind(sock_d, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) < 0){
        perror("server bind");
        exit(1);
    }

    /* become a listening socket, with a backlog of 5 */
    listen(sock_d, 5);

    // begin listening loop
    while(1)
    {
        cli_addrlen = sizeof(cli_addr);
        acc_sd = accept(sock_d, (struct sockaddr*) &cli_addr, &cli_addrlen);

        /* fork to handle client request */
        if((pid = fork()) < 0){ // error
            perror("fork");
            exit(1);
        } else if(pid > 0){ // i am the parent
            close(acc_sd);
            continue;
        } 

        /* To reach here the process must be a child */
        close(sock_d);
        /* serve the client */
        server_a_client(acc_sd);
        /* exit child process */
        exit(0);

    }

    return 0;
}

void server_a_client(int sock_d)
{
    int nr, nw;
    char buf[MAX_BLOCK_SIZE];

    char return_msg[MAX_BLOCK_SIZE] = "Hello from server!";

    while(1){
        /*read data from client*/
        if((nr = readn(sock_d, buf, MAX_BLOCK_SIZE)) <= 0){
            return; /* connection broke down */
        }

        nw = writen(sock_d, return_msg, sizeof(return_msg));
    }
}
