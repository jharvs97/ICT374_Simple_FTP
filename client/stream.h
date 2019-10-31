/**
 * @author Hong Xie, altered by Joshua Harvey
 * @brief Functions to write one, two, four, and N amount of bytes
 */


#define MAX_BLOCK_SIZE (1024*5)    /* maximum size of any piece of */
                                   /* data that can be sent by client */


/**
 * @brief Read a stream of bytes from fd to buf
 * @param fd (int) File descriptor
 * @param buf (char*) buffer
 * @param bufsize (int) size of buffer in bytes
 * @return number of bytes read
 */
int readn(int fd, char *buf, int bufsize);


    

/**
 * @brief Write a stream of bytes from buf to fd
 * @param fd (int) File descriptor
 * @param buf (char*) buffer
 * @param bufsize (int) size of buffer in bytes
 * @return number of bytes read
 */
int writen(int fd, char *buf, int nbytes);

/**
 * @brief Write a single byte to fd
 * @param fd (int) Socket file descriptor
 * @param code (char) Opcode
 * @return number of bytes written
 */
int write_opcode(int fd, char code);

/**
 * @brief Read a single byte from fd
 * @param fd (int) Socket file descriptor
 * @param code (char) Opcode
 * @return number of bytes read
 */
int read_opcode(int fd, char* code);

/**
 * @brief Write two bytes (short) in network byte order to fd
 * @param fd (int) Socket file descriptor
 * @param data (short) Two bytes of data
 * @return number of bytes read
 */
int write_twonetbs(int fd, short data);

/**
 * @brief Read two bytes (short) in network byte order from fd
 * @param fd (int) Socket file descriptor
 * @param data (short) Two bytes of data
 * @return number of bytes read
 */
int read_twonetbs(int fd, short *data);

/**
 * @brief Write four bytes (int) in network byte order to fd
 * @param fd (int) Socket file descriptor
 * @param data (int) Four bytes of data
 * @return number of bytes read
 */
int write_fournetbs(int fd, int data);

/**
 * @brief Read four bytes (int) in network byte order from fd
 * @param fd (int) Socket file descriptor
 * @param data (int) Four bytes of data
 * @return number of bytes read
 */
int read_fournetbs(int fd, int *data);