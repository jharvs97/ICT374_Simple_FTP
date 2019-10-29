
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
#include <time.h>
#include <stdio.h>
#include <stdarg.h>

// My headers
#include "daemon.h"
#include "stream.h"
#include "protocol.h"

#define SERV_TCP_PORT 40005

#define LOGGER_PATH "./server.log"

void server_a_client(int sock_d);

char* mapCmdToString(char cmd)
{
    char* cmd_str = malloc(32);

    switch(cmd){
        case PWD_OPCODE:
            strcpy(cmd_str, "PWD");
            break;
        case CD_OPCODE:
            strcpy(cmd_str, "CD");
            break;
        case DIR_OPCODE:
            strcpy(cmd_str, "DIR");
            break;
        default:
            return NULL;
            break;
    }   

    return cmd_str;
}

/**
 * @brief Fuction to log to file
 * @param msg Message to send to logging file
 * @return void
 */
void my_log(char cmd, char* msg)
{
    int fd;
    struct tm* tm_ptr;
    time_t lt;

    if( (fd = open(LOGGER_PATH, O_WRONLY | O_APPEND | O_CREAT, 0766)) < 0 ){
        perror("logger can't write");
        exit(0);
    }

    lt = time(NULL);
    tm_ptr = localtime(&lt);    
    char* time_str = asctime(tm_ptr);

    char* cmd_str = mapCmdToString(cmd);
    
    dprintf(fd, "[%s]: <%s> %s\n", strtok(time_str, "\n"), cmd_str, msg);
    
    free(cmd_str);

    close(fd);    
}

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
            if(strcmp(ent_p->d_name, ".") == 0)
                continue;
            
            if(strcmp(ent_p->d_name, "..") == 0)
                continue;
            
            strcat(files_buf, ent_p->d_name);
            strcat(files_buf, "\n");
        }
        len = (short) strlen(files_buf);
        files_buf[len-1] = '\0';
        closedir(dir_p);
    }

    if((nw = write_opcode(sock_d, DIR_OPCODE)) < 0){
        my_log(DIR_OPCODE, "Failed writing Opcode");
        return;
    }

    if((nw = write_twonetbs(sock_d, len)) < 0){
        my_log(DIR_OPCODE, "Failed writing buffer length");
        return;
    }

    if((nw = writen(sock_d, files_buf, len)) < 0){
        my_log(DIR_OPCODE, "Failed writing buffer");
        return;
    }
    
    my_log(DIR_OPCODE, "DIR finished");
    return;
}

void serve_pwd(int sock_d){
    int len;

    char buf[256];
    // Ensure the buffer is empty
    bzero(buf, 256);
    getcwd(buf, sizeof(buf));

    if(write_opcode(sock_d, PWD_OPCODE) < 0){
        my_log(PWD_OPCODE, "Failed to write OPcode to client");
        return;
    }

    if(write_twonetbs(sock_d, (short) strlen(buf)) < 0){
        my_log(PWD_OPCODE, "Failed to write buffer length to client");
        return;
    }

    if(writen(sock_d, buf, strlen(buf)) < 0){
        my_log(PWD_OPCODE, "Failed to write buffer to client");
        return;
    }

    my_log(PWD_OPCODE, "PWD finished");

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
        my_log(CD_OPCODE, "Failed to read length of directory");
        return;
    }

    // Read the dir buffer
    if(readn(sock_d, dir, (int) len) < 0){
        my_log(CD_OPCODE, "Failed to read the directory buffer");
        return;
    }

    if(strlen(dir) > 0){
        if(chdir(dir) == -1){
            my_log(CD_OPCODE, "Directory doesn't exist");
            ack = CD_ACK_NOEXIST;
        } else {
            ack = CD_ACK_SUCCESS;
        }
    }

    if(write_opcode(sock_d, CD_OPCODE) < 0){
        my_log(CD_OPCODE, "Failed to write OP code");
        return;
    }

    if(write_opcode(sock_d, ack) < 0){
        my_log(CD_OPCODE, "Failed to write ack code");
        return;
    }

    my_log(CD_OPCODE, "CD finished");
    
    return;
}

void serve_put(int sock_d){
	short chunk = 0;
	char filename[256];
	char *block_buf = malloc(MAX_BLOCK_SIZE);
	short len = 0;
	char op = 'A';

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
        	FILE *fstr = NULL;
	
		//if remote end can open the file ready for sending, procee
		if (read_opcode(sock_d, &op) < 0){
			free(block_buf);
			return;
         	}
	 	if (op != PUT_STATUS_READY){
			//anything other than an READY abort
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
		if(write_opcode(sock_d, PUT_STATUS_READY) < 0){
        		fclose(fstr);
	        	free(block_buf);
			return;
    		}
		//set chunk to a positive value to start while loop
		chunk = 1;
		int iteration = 0;
		while (chunk > 0){
			read_twonetbs(sock_d, &chunk);
			//this catches the exception of a zero byte file, sends an acknowledgement without a payload
			if (chunk == 0 && iteration == 0){
				write_opcode(sock_d, PUT_STATUS_OK);
			}
			if (chunk > 0){
				readn(sock_d, block_buf, (int) chunk);
				fwrite(block_buf, 1, chunk, fstr);
				write_opcode(sock_d, PUT_STATUS_OK);
			}
			iteration++;
        	
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
	char op = 'A';
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
		FILE *fstr = NULL
		struct stat file_stat;
		int transfer_remain = 0;
		int file_offset = 0;
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
			//otherwise, all okay at this end
			if(write_opcode(sock_d, GET_STATUS_READY) < 0){
				free(block_buf);
				fclose(fstr);
				return;
			}
			//is other end okay?
			//read code
			if (read_opcode(sock_d, &op) < 0){
				free(block_buf);
				fclose(fstr);
				return;
			}	
			//if other end isn't ready, abort. 
			if (op != GET_STATUS_READY){
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
			else 
			{
			//if file size is zero bytes just wait for ackowledgment
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
		else
		{
			//stat file failed
			write_opcode(sock_d, GET_STATUS_ERR);
	                free(block_buf);
			return;
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

        my_log(opcode, "Recieved OPcode from client");
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

