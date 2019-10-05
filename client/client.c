#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>

// My headers
#include "token.h"
#include "stream.h"

#define SERV_TCP_PORT 40005

#define h_addr h_addr_list[0]

#define u_long unsigned long

int main(int argc, char** argv)
{
    int sock_d;                     // Socket descriptor
    char buf[MAX_BLOCK_SIZE];       // input buffer
    char host[60];                  // host address as string 
    unsigned short port;            // port number
    struct sockaddr_in ser_addr;    // server address
    struct hostent *hp;             // host information
    char* tokens[MAX_NUM_TOKENS];   // tokens of the input command
    char temp_buf[MAX_BLOCK_SIZE];  // Create a temp buffer since the tokenisation will malform the input string
    char files_buf[MAX_BLOCK_SIZE];
    int num_tokens;                 // Number of tokens after a line has been tokenised
    int nw, nr;

    // Connect to local host socket for now...
    if(argc == 1){
        // If no host:port is provided, assume local host 
        gethostname(host, sizeof(host));
        port = SERV_TCP_PORT;   
    }
    else if(argc == 2){
        // Use the host/IP provided
        strcpy(host, argv[1]);
        port = SERV_TCP_PORT;        
    }
    else{
        printf("Usage: %s [ <server_host_name> | <server_ip_address> ]\n", argv[0]);
        exit(1);
    }

    // Get host addr and build server socket address
    bzero((char*) &ser_addr, sizeof(ser_addr));
    ser_addr.sin_family = AF_INET;
    ser_addr.sin_port = htons(port);
    
    // If you can get the host address by a name
    // Then set the sin_addr to hp->h_addr
    if((hp = gethostbyname(host)) != NULL){
        ser_addr.sin_addr.s_addr = *(u_long*)hp->h_addr;
    } else {
        printf("Invalid hostname\n"); exit(1);
    }

    // If address couldn't be resolved by gethostbyname()
    // Check if host is an IP address string
    // If inet_aton is non-zero, success
    if((inet_aton(host, &ser_addr.sin_addr)) == 0){
        printf("Invalid IP address\n"); exit(1);
    }
    

    // create TCP socket
    sock_d = socket(PF_INET, SOCK_STREAM, 0);

    // connect socket to server
    if(connect(sock_d, (struct sockaddr *) &ser_addr,sizeof(ser_addr)) < 0){
        perror("client connect\n"); exit(1);
    }

    while(1){
        printf("$ ");

        fgets(buf, sizeof(buf), stdin);
        
        nr = strlen(buf);
        if(buf[nr-1] == '\n'){
            buf[nr-1] = '\0';
            --nr;
        }
       

        if(strcmp(buf, "quit") == 0){
            printf("Thanks for using our FTP solution!\nExiting now . . .\n");
            exit(0);
        }

        if(nr > 0){
            
            memset(temp_buf, 0, MAX_BLOCK_SIZE);
            memcpy(temp_buf, buf, MAX_BLOCK_SIZE);
            num_tokens = tokenise(temp_buf, tokens);
            /* LOCAL COMMAND HANDLER */
            // TODO: Refactor handlers into their own module perhaps?
            if(strcmp(tokens[0], "lpwd") == 0){
                memset(buf, 0, MAX_BLOCK_SIZE);
                getcwd(buf, MAX_BLOCK_SIZE);
                printf("%s\n", buf);
            }
            else if(strcmp(tokens[0], "lcd") == 0){
                if(num_tokens == 2){
                    if(chdir(tokens[1]) == -1){
                        printf("Error: directory not found\n");
                    }
                } else {
                    printf("Invalid command. Usage is: lcd [<path>]\n");
                }
            }
            else if(strcmp(tokens[0], "ldir") == 0){
                DIR* dir_p;
                struct dirent* dirent_p;

                memset(files_buf, 0, MAX_BLOCK_SIZE);

                if((dir_p = opendir(".")) == NULL){
                    perror("opening dir");
                } else {
                    //strcat(files_buf, "\n");
                    while( (dirent_p = readdir(dir_p)) != NULL){
                        if(strcmp(dirent_p->d_name, ".") == 0)
                            continue;
                        
                        if(strcmp(dirent_p->d_name, "..") == 0)
                            continue;

                        strcat(files_buf, dirent_p->d_name);
                        strcat(files_buf, "\n");
                        
                    }

                    printf("%s", files_buf);
                }
            }
        }
    }
    return 0;
}

