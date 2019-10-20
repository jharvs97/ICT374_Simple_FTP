#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
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
#include "protocol.h"

#define SERV_TCP_PORT 40005

#define h_addr h_addr_list[0]

#define u_long unsigned long

void local_pwd(){
    char buf[MAX_BLOCK_SIZE];
    memset(buf, 0, MAX_BLOCK_SIZE);
    getcwd(buf, MAX_BLOCK_SIZE);
    printf("%s\n", buf);
}

void local_cd(char* dir){
    if(chdir(dir) == -1){
        printf("Error: directory not found\n");
    }
}

void local_dir(){
    DIR* dir_p;
    struct dirent* dirent_p;
    char files_buf[MAX_BLOCK_SIZE];

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

void send_dir(int sock_d){
    char op = 0;
    short nr=0; 
    int nw=0;
    char files_buf[MAX_BLOCK_SIZE]; 

    // Write the OpCode 
    if(write_opcode(sock_d, DIR_OPCODE) < 0){
        printf("Failed to send dir\n"); return;
    }

    // Read the OpCode from server
    if(read_opcode(sock_d, &op) < 0){
        printf("%c\n", op);
        printf("Failed to read OpCode\n"); return;
    }

    // Read the length of dir string
    if(read_twonetbs(sock_d, &nr) < 0){
        printf("Failed to read filesize\n"); return;
    }

    // Read in the bytes of the dir string
    if(readn(sock_d, files_buf, (int) nr) < 0){
        printf("Failed to read directory\n"); return;
    }

    files_buf[nr] = '\0';

    printf("%s\n", files_buf);
}

void send_pwd(int sock_d){

    char op;
    short len;

    if(write_opcode(sock_d, PWD_OPCODE) < 0){
        printf("Failed to send OpCode!\n"); return;
    }

    if(read_opcode(sock_d, &op) < 0){
        printf("Failed to read OpCode!\n"); return;
    }

    if(op != PWD_OPCODE){
        printf("Wrong OpCode recieved\n"); return;
    }

    if(read_twonetbs(sock_d, &len) < 0){
        printf("Failed to read message length\n"); return;
    }
    
    char pwd_buf[len+1];

    if(readn(sock_d, pwd_buf, (int) len) < 0){
        printf("Failed to read pwd from server!\n"); return;
    }

    pwd_buf[len] = '\0';

    printf("%s\n", pwd_buf);
}

void send_cd(int sock_d, char* dir){

    char op;
    char ack;
    int len = strlen(dir);

    if(write_opcode(sock_d, CD_OPCODE) < 0){
        printf("Failed to send OpCode!\n"); return;
    }

    if(write_twonetbs(sock_d, (short) len) < 0){
        printf("Failed to write buffer length\n"); return;
    }

    if(writen(sock_d, dir, len) < 0){
        printf("Failed writing buffer\n"); return;
    }

    if(read_opcode(sock_d, &op) < 0){
        printf("Failed reading OpCode\n"); return;
    }

    if(op != CD_OPCODE){
        printf("Wrong OpCode!\n"); return;
    }

    if(read_opcode(sock_d, &ack) < 0){
        printf("Failed reading Ack code\n"); return;
    }

    switch(ack){
        case CD_ACK_SUCCESS:
            break;
        case CD_ACK_NOEXIST:
            printf("Directory doesn't exist!\n");
            break;
        default:
            break;
    }

    return;
}

void send_put(int sock_d, char* filename ){

    char op;
    char ack;
    char *block_buf = malloc(MAX_BLOCK_SIZE);
    int len = strlen(filename);
    //clear buffer
    bzero(block_buf, MAX_BLOCK_SIZE);


    if(write_opcode(sock_d, PUT_OPCODE) < 0){
        printf("Failed to send OpCode!\n"); return;
    }

    if(write_twonetbs(sock_d, (short) len) < 0){
        printf("Failed to write buffer length\n"); return;
    }

    if(writen(sock_d, filename, len) < 0){
        printf("Failed writing buffer\n"); return;
    }

    if(strlen(filename) > 0){
        char *fname = filename;
        FILE *fstr;
        struct stat file_stat;
	int transfer_remain;
	int file_offset;

	if(stat(fname, &file_stat) == 0){
		//get size of transfer in bytes
		transfer_remain = file_stat.st_size;
	        //try and open file read only
        	fstr = fopen(fname, "r" );
		if (fstr == NULL){
			printf("1 - Local - Could not open file for reading\n");
			//send sad flag and exit
			if(write_opcode(sock_d, PUT_STATUS_ERR) < 0){
                        	free(block_buf);
				return;
                	}
			free(block_buf);
			return;
		}
		//when we have confirmed we can stat and open the file, send status
		if(write_opcode(sock_d, PUT_STATUS_OK) < 0){
                	free(block_buf);
			fclose(fstr);
			return;
        	}
		
		//wait for indication that the other end has been able to open file for writing
		if (read_opcode(sock_d, &op) < 0){
			free(block_buf);
                        fclose(fstr);
			return;
		}
		if (op != PUT_STATUS_OK){
			if (op == PUT_STATUS_ERR)
			{
				printf ("2 - Remote - Could not open for writing: %s\n", fname);
				fclose(fstr);
				free(block_buf);
				return;
			}
			else{
				printf("2 - Unknown flag from remote end");
				free(block_buf);
				fclose(fstr);
				return;
			}
		}
		printf("Uploading %d bytes..", transfer_remain);
		//if there is more data than max block size, send a max sized block, otherwise send remaining data
		int dotcounter = 0;
		if (transfer_remain > 0){
			while (transfer_remain > 0){
				dotcounter += 1;
				if ( dotcounter % 10 == 0 ){
					printf(".");
					//fflush pushes the dots to stdout immediately
					fflush(stdout);
				}
				if (transfer_remain > MAX_BLOCK_SIZE){
					fread(block_buf, 1, MAX_BLOCK_SIZE, fstr);
					write_twonetbs(sock_d, (short) MAX_BLOCK_SIZE);
			                writen(sock_d, block_buf, MAX_BLOCK_SIZE);	
					transfer_remain = transfer_remain - MAX_BLOCK_SIZE;
					if (read_opcode(sock_d, &op) < 0){
                        			free(block_buf);
                        			fclose(fstr);
						return;
        	        		}
                			if (op != PUT_STATUS_OK){
                        			printf ("3 - Transfer issue... abort\n");
						free(block_buf);
                        			fclose(fstr);
	        	                	return;
        	        		}

				}
				else
				{
					fread(block_buf, 1, transfer_remain, fstr);
                                	write_twonetbs(sock_d, (short) transfer_remain);
	                                writen(sock_d, block_buf, transfer_remain);
        	                        transfer_remain = 0;
                	            	//following zero size write breaks look at other end
					write_twonetbs(sock_d, (short) transfer_remain);
					if (read_opcode(sock_d, &op) < 0){
	                                	free(block_buf);
        	                		fclose(fstr);
						return;
                        	        }
	                                if (op != PUT_STATUS_OK){
        	                                free(block_buf);
                	        		fclose(fstr);
						printf ("4 - Transfer issue... abort\n");
                                	return;
	                                }
	
				}
			
			}
		}
		else
		{
			//if file size is zero bytes just send zero size for ackowledgment
                        if (write_twonetbs(sock_d, (short) transfer_remain) < 0){
				printf("could not send size\n");
				free(block_buf);
                        	fclose(fstr);
                        	return;
			}
                        if (read_opcode(sock_d, &op) < 0){
                        printf("expected opcode didn't arrive, expected PUT_STATUS_OK\n");
                               free(block_buf);
                               fclose(fstr);
                               return;
                        }
                }
		printf("Complete!!\n");
	}
	else{
		//could not stat file
		printf("5 - Local - Could not open file\n");
		free(block_buf);
                fclose(fstr);
		return;
	}
	}

    free(block_buf);
    return;
}


void send_get(int sock_d, char* filename ){

    char op;
    char ack;
    char *block_buf = malloc(MAX_BLOCK_SIZE);
    int len = strlen(filename);
    int transfer_remain = 0;
    short chunk;
    bzero(block_buf, MAX_BLOCK_SIZE);

    if(write_opcode(sock_d, GET_OPCODE) < 0){
        printf("1 - Failed to send OpCode!\n"); 
	free(block_buf);
	return;
    }

    if(write_twonetbs(sock_d, (short) len) < 0){
        printf("2 - Failed to write buffer length\n"); 
	free(block_buf);
	return;
    }

    if(writen(sock_d, filename, len) < 0){
        printf("3 - Failed writing buffer\n"); 
	free(block_buf);
	return;
    }

    if(len > 0){
        char *fname = filename;
        FILE *fstr;
        int file_offset;
	if (read_opcode(sock_d, &op) < 0){
                free(block_buf);
		return;
        }
        //no point in wiping out a local file until we know the other end is ready to send a replacement. 
	if (op == GET_STATUS_OK){
        	printf ("6 - File opened on the remote end: %s\n", fname);
	        fstr = fopen(fname, "w" );
		fclose(fstr);
		//reopen now-empty file in append mode
		fstr = fopen(fname, "a" );
		if (fstr == NULL){
			printf ("7 - Could not opened file for writing: %s\n", fname);
			free(block_buf);
	                return;
		}
		//get total size of transfer - for reporting size only 
		if(read_fournetbs(sock_d, &transfer_remain) < 0){
			printf("8 - Could not get size of remote file: %s\n", fname);
			fclose(fstr);        		
			free(block_buf);
			return;
    		}
                printf("Downloading %d bytes..\n", transfer_remain);
               	fflush(stdout);
		//set chunk to a positive value to start while loop, it will be overwritten by receiving chunk size
		chunk = 1;
		int dotcounter = 0;
		while (chunk > 0){
			dotcounter += 1;
			read_twonetbs(sock_d, &chunk);
                	if ( dotcounter % 10 == 0 ){
				printf(".");fflush(stdout);
			}
			if (chunk > 0){
                        	readn(sock_d, block_buf, (int) chunk);
                        	fwrite(block_buf, 1, chunk, fstr);
                        	write_opcode(sock_d, GET_STATUS_OK);
                	}
			else
			{
				write_opcode(sock_d, GET_STATUS_OK);
			}
			

                }
		fclose(fstr);
                printf("Complete!!\n");
       	}
	else
	{
		//remote end has send an error when attempting to stat and then open file
		printf("12 - Remote Error - Cannot open file\n");
	}
    free(block_buf);

    }
    return;
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
    
    // create TCP socket
    sock_d = socket(PF_INET, SOCK_STREAM, 0);

    // connect socket to server
    if(connect(sock_d, (struct sockaddr *) &ser_addr,sizeof(ser_addr)) < 0){
        perror("client connect\n"); exit(1);
    }

    while(1){
        memset(buf, 0, sizeof(buf));
        //memset(temp_buf, 0, sizeof(temp_buf));

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
            
            //strcpy(temp_buf, buf);
            num_tokens = tokenise(buf, tokens);

            if(strcmp(tokens[0], "lpwd") == 0){
                local_pwd();
            }
            else if(strcmp(tokens[0], "lcd") == 0){
                if(num_tokens == 2){
                    local_cd(tokens[1]);
                } else {
                    printf("Invalid command. Usage is: lcd [<path>]\n");
                }
            }
            else if(strcmp(tokens[0], "ldir") == 0){
                local_dir();
            }
            else if(strcmp(tokens[0], "dir") == 0){
                send_dir(sock_d);
            }
            else if(strcmp(tokens[0], "pwd") == 0){
                send_pwd(sock_d);   
            }
            else if(strcmp(tokens[0], "cd") == 0){
                if(num_tokens == 2){
                    send_cd(sock_d, tokens[1]);
                }
                else {
                    printf("Invalid command. Usage is: cd [<path>]\n");
                }
            }
            else if(strcmp(tokens[0], "put") == 0){
                if(num_tokens == 2){
                    send_put(sock_d, tokens[1]);
                } else {
                    printf("Invalid command. Usage is: put [<filename>]\n");
                }
            }
	    else if(strcmp(tokens[0], "get") == 0){
                if(num_tokens == 2){
                    send_get(sock_d, tokens[1]);
                } else {
                    printf("Invalid command. Usage is: get [<filename>]\n");
                }
            }

        }
    }
    return 0;
}

