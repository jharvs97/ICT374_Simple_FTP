/*
 *  stream.c  -	(Topic 11, HX 22/5/1995)
 *	 	routines for stream read and write. 
 */

#include  <sys/types.h>
#include  <netinet/in.h> /* struct sockaddr_in, htons(), htonl(), */
#include  <unistd.h>
#include  <stdint.h>
#include  "stream.h"

int readn(int fd, char *buf, int bufsize)
{
    // short data_size;    /* sizeof (short) must be 2 */ 
    // int n, nr, len;

    // /* check buffer size len */
    // if (bufsize < MAX_BLOCK_SIZE)
    //      return (-3);     /* buffer too small */

    // /* get the size of data sent to me */
    // if (read(fd, (char *) &data_size, 1) != 1) return (-1);
    // if (read(fd, (char *) (&data_size)+1, 1) != 1) return (-1);
    // len = (int) ntohs(data_size);  /* convert to host byte order */ 

    // /* read len number of bytes to buf */
    // for (n=0; n < len; n += nr) {
    //     if ((nr = read(fd, buf+n, len-n)) <= 0) 
    //         return (nr);       /* error in reading */
    // }
    // return (len); 

    int nr=0, n;
    for(n=0; n < bufsize; n += nr){
        if ((nr = read(fd, buf+n, bufsize-n)) <= 0){
            return (nr);
        }
    }

    return n;
}

int writen(int fd, char *buf, int nbytes)
{
    // short data_size = nbytes;     /* short must be two bytes long */
    // int n, nw;

    // if (nbytes > MAX_BLOCK_SIZE) 
    //      return (-3);    /* too many bytes to send in one go */ 

    // /* send the data size */
    // data_size = htons(data_size);  
    // if (write(fd, (char *) &data_size, 1) != 1) return (-1);      
    // if (write(fd, (char *) (&data_size)+1, 1) != 1) return (-1);       

    // /* send nbytes */
    // for (n=0; n<nbytes; n += nw) {
    //      if ((nw = write(fd, buf+n, nbytes-n)) <= 0)  
    //          return (nw);    /* write error */
    // } 
    // return (n);

    int nw=0, n=0;
    for(n=0;n < nbytes; n+=nw){
        if ((nw = write(fd, buf+n, nbytes-n)) <= 0){
            return (nw);
        }
    }

    return n;
}

int write_opcode(int fd, char code){
    int nw = write(fd,&code, 1);

    if(nw != 1) return -1;

    return nw;  
}

int read_opcode(int fd, char* code){
    char d;
    int nr;
    if((nr = read(fd, &d, 1)) != 1) return -1;

    *code = d;

    return nr;
}

int write_twonetbs(int fd, short data){

    uint16_t ns_data = htons(data);

    int nw = write(fd,(char*) &ns_data, 2);

    if(nw != 2) return -1;

    return nw;
}

int read_twonetbs(int fd, short *data){

    uint16_t d = 0;
    int nr;
    

    if( (nr = read(fd,(char*) &d, 2)) != 2) return -1;

    *data = ntohs(d);
    
    return nr;
}

int write_fournetbs(int fd, int data){
    uint32_t d = htonl(data);

    int nw = write(fd,(char*)&d, 4);

    if(nw != 4) return -1;

    return nw;
}

int read_fournetbs(int fd, int *data){
    uint32_t d = 0, nr;

    if( (nr = read(fd, (char*)&d, 4)) != 4) return -1;

    *data = ntohl(d);

    return nr;
}