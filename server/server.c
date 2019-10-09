
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
#include "stream.h"
#include "protocol.h"

#define SERV_TCP_PORT 40005

void server_a_client(int sock_d);

void serve_dir(int sock_d){

    int len,nw;
    DIR* dir_p;
    struct dirent* ent_p;
    char files_buf[MAX_BLOCK_SIZE];

    dir_p = opendir(".");
    if(dir_p){
        while((ent_p = readdir(dir_p)) != 0){
            strcat(files_buf, ent_p->d_name);
            strcat(files_buf, "\n");
        }
        len = strlen(files_buf);
        files_buf[len-1] = '\0';
        closedir(dir_p);
    }

    if((nw = write_opcode(sock_d, DIR_OPCODE)) < 0){
        return;
    }

    if((nw = write_fournetbs(sock_d, len)) < 0){
        return;
    }

    if((nw = writen(sock_d, files_buf, len)) < 0){
        return;
    }

    return;
}

void serve_pwd(int sock_d){
    int len;

    char buf[256];
    getcwd(buf, sizeof(buf));
    // printf("%s\n", buf);

    len = strlen(buf);
    buf[len] = '\0';

    if(write_opcode(sock_d, PWD_OPCODE) < 0){
        return;
    }

    if(write_fournetbs(sock_d, len) < 0){
        return;
    }

    if(writen(sock_d, buf, len) < 0){
        return;
    }

    return;
}

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
    char opcode;

    while(read_opcode(sock_d, &opcode) > 0){
        /*read data from client*/
        // if((nr = readn(sock_d, buf, MAX_BLOCK_SIZE)) <= 0){
        //     return; /* connection broke down */
        // }
        // nw = writen(sock_d, return_msg, sizeof(return_msg));

        /* Read OpCode from client */
        // if( (nr = read_opcode(sock_d, &opcode)) <= 0){
        //     return;files_bu
        // }

        switch(opcode){
            case DIR_OPCODE:
                serve_dir(sock_d);
                break;
            case PWD_OPCODE:
                serve_pwd(sock_d);
                break;
            default:
                break;
        }
        
    }
}

