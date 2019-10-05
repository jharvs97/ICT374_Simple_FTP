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

int send_server(int fd, char *buf, int nbytes){
	short data_size = nbytes; /* short must be two bytes long */
	int n, nw;
	if (nbytes > MAX_BLOCK_SIZE)
	return (-3); /* too many bytes to send in one go */
	/* send the data size */
	data_size = htons(data_size);
	if (write(fd, (char *) &data_size, 1) != 1) return (-1);
	if (write(fd, (char *) (&data_size)+1, 1) != 1) return (-1);
	/* send nbytes */
	for (n=0; n<nbytes; n += nw) {
	if ((nw = write(fd, buf+n, nbytes-n)) <= 0)
	return (nw); /* write error */
	}
	return (n);
}

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
        printf("Hostname: %s", host);
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
	    else if(strcmp(tokens[0], "cwd") == 0){
	    	switch(num_tokens){
			case 1 :
				printf("No directory specified");
				break;
			case 2 :
                     		printf("length of token 1: %d\n",(int)strlen(tokens[1]));
				short length = (short)strlen(tokens[1]);
				char command = 'C';
				command = htons(command);
				strcpy(buf,tokens[1]);
				//send command
				send_server(sock_d, &command, sizeof(command));
				break;

			default :
				printf("Directory names cannot have spaces");
	    }
	    }
        }
	}	    
    return 0;
}
