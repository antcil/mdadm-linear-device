#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <err.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include "net.h"
#include "jbod.h"
#include <assert.h>
#include <fcntl.h>

/* the client socket descriptor for the connection to the server */
int cli_sd = -1;

/* attempts to read n bytes from fd; returns true on success and false on
 * failure */
static bool nread(int fd, int len, uint8_t *buf) {
  //Keeps track of bytes read
  int bytes_read = 0;
  // Reads the buffer in a loop to ensure all bytes are read
  while (bytes_read < len) {
    int current_read = read(fd, &buf[bytes_read], len - bytes_read);
    if (current_read < 0) {
      return false;
    }
    bytes_read += current_read;
  }
  return true;
}

/* attempts to write n bytes to fd; returns true on success and false on
 * failure */
static bool nwrite(int fd, int len, uint8_t *buf) {
  //Keeps track of bytes written
  int bytes_written = 0;
  // Reads the buffer in a loop to ensure all bytes are written
  while (bytes_written < len) {
    int current_write = write(fd, &buf[bytes_written], len - bytes_written);
    if (current_write < 0) {
      return false;
    }
    bytes_written += current_write;
  }

  return true;
}

/* attempts to receive a packet from fd; returns true on success and false on
 * failure */
static bool recv_packet(int fd, uint32_t *op, uint16_t *ret, uint8_t *block) {
  uint8_t header[HEADER_LEN];
  if (!nread(fd, HEADER_LEN, header)) {
    return false;
  }
  // Attains length, opcode and return value of packet
  uint16_t len;
  memcpy(&len, header, sizeof(uint16_t));
  memcpy(op, header+2, sizeof(uint32_t));
  memcpy(ret, header+6, sizeof(uint16_t));
  //Once copy use ntoh functions
  len = ntohs(len);

  // Checks if there is a block
  if (len > 8) {
    // Reads block
    if (!nread(fd, 256, block)) {
      return false;
    }
  }
  return true;
}

/* attempts to send a packet to sd; returns true on success and false on
 * failure */
static bool send_packet(int sd, uint32_t op, uint8_t *block) {
  // Determines length of block based on op code
  u_int16_t len = 8;
  if (op >> 26 == JBOD_WRITE_BLOCK) {
    len = 264;
  }

  uint8_t sockbuf[len];
  // Convert variables to network
  len = htons(len);
  op = htonl(op);
  // Build header
  memcpy(sockbuf, &len, sizeof(uint16_t));
  memcpy(sockbuf+2, &op, sizeof(uint32_t));
  //Copy block to buffer
  if (ntohl(op) >> 26 == JBOD_WRITE_BLOCK) {
    memcpy(sockbuf + 8, block, JBOD_BLOCK_SIZE);
  }
  //Write packet to socket
  if (!nwrite(sd, (int)ntohs(len), sockbuf)) {
    return false;
  }
  return true;
}

/* attempts to connect to server and set the global cli_sd variable to the
 * socket; returns true if successful and false if not. */
bool jbod_connect(const char *ip, uint16_t port) {
  // create IPv4 structure
  struct sockaddr_in s1;
  memset(&s1, 0, sizeof(s1));
  s1.sin_family = AF_INET;
  s1.sin_addr.s_addr = inet_addr(ip);
  s1.sin_port = htons(port);
  // Create a socket
  cli_sd = socket(AF_INET, SOCK_STREAM, 0);
  if (cli_sd == -1) {
    printf( "Error on socket creation [%s]\n", strerror(errno) );
    return false;
  }
  // Converts ip to binary form and checks if it was successful
  if (inet_pton(AF_INET, ip, &(s1.sin_addr)) <= 0) {
    printf( "Error on ip conversion [%s]\n", strerror(errno) );
    close(cli_sd);
    return false; 
  }
  // Connects socket and checks if it was successful
  if (connect(cli_sd, (struct sockaddr *)&s1, sizeof(s1)) == -1) {
    printf( "Error on connecting socket [%s]\n", strerror(errno) );
    close(cli_sd);
    return false;
  }
  return true;
}

/* disconnects from the server and resets cli_sd */
void jbod_disconnect(void) {
  close(cli_sd);
  cli_sd = -1;
}

/* sends the JBOD operation to the server and receives and processes the
 * response. */
int jbod_client_operation(uint32_t op, uint8_t *block) {
  // Write packet to server
  if (!send_packet(cli_sd, op, block)) {
    return -1;
  }

  // Initialize variables for response 
  uint32_t rop;
  uint16_t ret;

  // Attempts to receive response
  if (!recv_packet(cli_sd, &rop, &ret, block)) {
    return -1;
  }
  return 0;
}
