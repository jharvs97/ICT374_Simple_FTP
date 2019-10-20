
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <dirent.h>
#include <arpa/inet.h>

// My headers
#include "daemon.h"
#include "stream.h"
#include "protocol.h"

#define SERV_TCP_PORT 40005

void server_a_client(int sock_d);

void serve_dir(int sock_d){

    short len;
    int nw;
    DIR* dir_p;
    struct dirent* ent_p;
    char files_buf[MAX_BLOCK_SIZE];
    bzero(files_buf, MAX_BLOCK_SIZE);

    dir_p = opendir(".");
    if(dir_p){
        while((ent_p = readdir(dir_p)) != 0){
            strcat(files_buf, ent_p->d_name);
            strcat(files_buf, "\n");
        }
        len = (short) strlen(files_buf);
        files_buf[len-1] = '\0';
        closedir(dir_p);
    }

    if((nw = write_opcode(sock_d, DIR_OPCODE)) < 0){
        return;
    }

    if((nw = write_twonetbs(sock_d, len)) < 0){
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
    // Ensure the buffer is empty
    bzero(buf, 256);
    getcwd(buf, sizeof(buf));

    if(write_opcode(sock_d, PWD_OPCODE) < 0){
        return;
    }

    if(write_twonetbs(sock_d, (short) strlen(buf)) < 0){
        return;
    }

    if(writen(sock_d, buf, strlen(buf)) < 0){
        return;
    }

    return;
}

void serve_cd(int sock_d){
    short len;
    char dir[256];
    // Ensure the buffer is empty
    bzero(dir, 256);
    char ack;

    // Read in length of the directory buffer
    if(read_twonetbs(sock_d, &len) < 0){
        return;
    }

    // Read the dir buffer
    if(readn(sock_d, dir, (int) len) < 0){
        return;
    }

    // printf("len = %d\n", len);
    // printf("DIR = %s\n", dir);

    //dir[strlen(dir)-1] = '\0';

    if(strlen(dir) > 0){
        if(chdir(dir) == -1){
            ack = CD_ACK_NOEXIST;
        } else {
            ack = CD_ACK_SUCCESS;
        }
    }

    if(write_opcode(sock_d, CD_OPCODE) < 0){
        return;
    }

    if(write_opcode(sock_d, ack) < 0){
        return;
    }
    
    
    return;
}

void serve_put(int sock_d){
    short chunk;
    char filename[256];
    char *block_buf = malloc(MAX_BLOCK_SIZE);
    short len;
    char op;

    // Ensure the buffers are empty
    bzero(filename, 256);
    bzero(block_buf, MAX_BLOCK_SIZE);
    

    // Read in length of the filename buffer
    if(read_twonetbs(sock_d, &len) < 0){
        free(block_buf);
	return;
    }

    // Read the filename buffer
    if(readn(sock_d, filename, (int) len) < 0){
        free(block_buf);
	return;
    }

    if(strlen(filename) > 0){
        char *fname = filename;
        FILE *fstr;
	
	//if remote end can open the file ready for sending, proceed
	 if (read_opcode(sock_d, &op) < 0){
                 free(block_buf);
		 return;
         }
	 if (op != PUT_STATUS_OK){
		 //anything other than an OKAY abort
		 free(block_buf);
		 return;
	 }
	
	//try and open a new file, or open and truncate
	//if fails send error flag
	fstr = fopen(fname, "w" );
	if (fstr == NULL){
		if(write_opcode(sock_d, PUT_STATUS_ERR) < 0){
			free(block_buf);
			return;
		}
		free(block_buf);
		return;
        }
	//then close new zero byte file and open in append mode
	//if fails send error flag
	fclose(fstr);
	fstr = fopen(fname, "a");
	if (fstr == NULL){
		if(write_opcode(sock_d, PUT_STATUS_ERR) < 0){
			free(block_buf);
			return;
                }
		free(block_buf);
                return;
	}
 	//otherwise, all okay	
	if(write_opcode(sock_d, PUT_STATUS_OK) < 0){
        	fclose(fstr);
	        free(block_buf);
		return;
    	}
	//set chunk to a positive value to start while loop
	chunk = 1;
	while (chunk > 0){
		read_twonetbs(sock_d, &chunk);
		if (chunk > 0){
			readn(sock_d, block_buf, (int) chunk);
			fwrite(block_buf, 1, chunk, fstr);
			//write_opcode(sock_d, PUT_STATUS_OK);
		}
		write_opcode(sock_d, PUT_STATUS_OK);
        	
    	}
	fclose(fstr);
    }

    free(block_buf);
    return;

}


void serve_get(int sock_d){
    short len = 0;
    char filename[256];
    char *block_buf = malloc(MAX_BLOCK_SIZE);
    // Ensure the buffers are empty
    bzero(filename, 256);
    bzero(block_buf, MAX_BLOCK_SIZE);
    char op;
    // Read in length of the filename buffer
    if(read_twonetbs(sock_d, &len) < 0){
        free(block_buf);
	return;
    }

    // Read the filename buffer
    if(readn(sock_d, filename, (int) len) < 0){
        free(block_buf);
	return;
    }
    
    if(strlen(filename) > 0){
        char *fname = filename;
        FILE *fstr;
	struct stat file_stat;
        int transfer_remain;
        int file_offset;
	//check if file exists, get size
	if ((stat(fname, &file_stat)) == 0){
		//get size of transfer in bytes
                transfer_remain = file_stat.st_size;
                //try and open file read only
        	//if fails send error flag
        	fstr = fopen(fname, "r" );
        	if (fstr == NULL){
                	if(write_opcode(sock_d, GET_STATUS_ERR) < 0){
                        	free(block_buf);
				return;
                	}
			free(block_buf);
			return;
		}
		//otherwise, all okay
        	if(write_opcode(sock_d, GET_STATUS_OK) < 0){
                	free(block_buf);
			fclose(fstr);
			return;
        	}
		//send total file size
		if (write_fournetbs(sock_d, transfer_remain) < 0){
			//couldn't write size
			free(block_buf);
                        fclose(fstr);
			return;
		}
		//if amount to send exceeds max block size, send a max size block. Otherwise send remaining data.
        	if (transfer_remain > 0){
			while (transfer_remain > 0){
                        	if (transfer_remain > MAX_BLOCK_SIZE){
                                	fread(block_buf, 1, MAX_BLOCK_SIZE, fstr);
                                	write_twonetbs(sock_d, (short) MAX_BLOCK_SIZE);
                                	writen(sock_d, block_buf, MAX_BLOCK_SIZE);
                                	transfer_remain = transfer_remain - MAX_BLOCK_SIZE;
                                	if (read_opcode(sock_d, &op) < 0){
                                        	//transfer issue - abort
                                		free(block_buf);
                        			fclose(fstr);
						return;
                                	}
                                	if (op != GET_STATUS_OK){
                                		//transfer issue - abort
						free(block_buf);
                        			fclose(fstr);
						return;
                                	}

                        	}	
                        	else
                        	{
                                	fread(block_buf, 1, (int) transfer_remain, fstr);
                                	write_twonetbs(sock_d, (short) transfer_remain);
                                	writen(sock_d, block_buf, transfer_remain);
                                	transfer_remain = 0;
                                	//following zero size write breaks look at other end
                                	write_twonetbs(sock_d, (short) transfer_remain);
                                	if (read_opcode(sock_d, &op) < 0){
                                        	//expected opcode didn't arrive, expected PUT_STATUS_OK
				        	free(block_buf);
                        			fclose(fstr);
						return;
                                	}
                                	if (op != GET_STATUS_OK){
                                        	//Transfer issue - abort
				        	free(block_buf);
                        			fclose(fstr);
						return;
                                	}

                        	}

                	}
			 if (read_opcode(sock_d, &op) < 0){
                         //printf("5 - expected opcode didn't arrive, expected PUT_STATUS_OK\n");
                               	free(block_buf);
                        	fclose(fstr);
				return;
                         }
                         if (op != GET_STATUS_OK){
                         //Transfer issue - abort
                                free(block_buf);
                        	fclose(fstr);
				return;
                         }

		}
		else {
		//if file size is zero bytes just wait for ackowledgment
			write_twonetbs(sock_d, (short) transfer_remain);
                        //writen(sock_d, block_buf, transfer_remain); 
			
			if (read_opcode(sock_d, &op) < 0){
                         //expected opcode didn't arrive, expected PUT_STATUS_OK
                                free(block_buf);
                        	fclose(fstr);
				return;
                         }
                         if (op != GET_STATUS_OK){
                         //Transfer issue - abort
                      		free(block_buf);
                        	fclose(fstr);   
		      		return;
                         }
			
		}
		
	   } 
	   else
	 {
	  //stat file failed
	  if(write_opcode(sock_d, GET_STATUS_ERR) < 0){
                       free(block_buf);
                       fclose(fstr);
		       return;
                }
       	 }
        fclose(fstr);
    }

    free(block_buf);
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
            case CD_OPCODE:
                serve_cd(sock_d);
		break;
	    case PUT_OPCODE:
		serve_put(sock_d);
                break;
	    case GET_OPCODE:
                serve_get(sock_d);
                break;
            default:
                break;
        }
        
    }
}

