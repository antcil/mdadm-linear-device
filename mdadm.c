#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "mdadm.h"
#include "jbod.h"
#include "net.h"

uint32_t encode_op (int cmd, int disk_num, int block_num) {
  uint32_t op = 0;
  // Pushes to get to desired bits
  op = (cmd << 26) | (disk_num << 22) | block_num;
  return op;
}

int is_mounted = 0;

int mdadm_mount(void) {
  if (is_mounted == 1) {
    return -1;
  }
  uint32_t op = encode_op(JBOD_MOUNT, 0, 0);
  jbod_client_operation(op, NULL);
  is_mounted = 1;
  return 1;
}

int mdadm_unmount(void) {
  if (is_mounted == 0) {
    return -1;
  }
  uint32_t op = encode_op(JBOD_UNMOUNT, 0, 0);
  jbod_client_operation(op, NULL);
  is_mounted = 0;
  return 1;
}

// Performs calculations based on address to find approporiate values
void translate_address(uint32_t addr, int *disk_num, int *block_num, int *offset) {
  // Divides by size of disk and only returns whole number to find correct disk number
  *disk_num = addr / JBOD_DISK_SIZE;
  // First divides by disk and obtains remainder to obtain number of bytes into a disk, then divides by block size for block number
  *block_num = (addr % JBOD_DISK_SIZE) / JBOD_BLOCK_SIZE;
  // Obtains number of byte into a disk and then number of bytes into an incomplete block for offset
  *offset = (addr % JBOD_DISK_SIZE) % JBOD_BLOCK_SIZE;
}

// Verifies that the disk_num and block_num are valid and seeks to them
int seek(int disk_num, int block_num) {
  uint32_t disk_seek = encode_op(JBOD_SEEK_TO_DISK, disk_num, 0);
  uint32_t block_seek = encode_op(JBOD_SEEK_TO_BLOCK, 0, block_num);
  if (!disk_seek || !block_seek) {
    return -1;
  }
  jbod_client_operation(disk_seek, NULL);
  jbod_client_operation(block_seek, NULL);
  return 1;
}

int min(int a, int b) {
  if (a < b || a == b) {
    return a;
  }
  else {
    return b;
  }
}

int mdadm_read(uint32_t addr, uint32_t len, uint8_t *buf) {
  int space = len + addr;
  // Covers error criteria
  if (is_mounted == 0 || len > 1024 || space > 1048576 || (buf == NULL && len > 0)) {
    return -1;
  }
  uint32_t current_addr = addr;
  int bytes_read = 0;
  int disk_num, block_num, offset;
  translate_address(addr, &disk_num, &block_num, &offset);

  if (seek(disk_num, block_num) != 1) {
    return -1;
  }

  while (space > current_addr) {
    // Searches the cache if enabled and increments variables
    if (cache_lookup(disk_num, block_num, buf) == 1) {
      int block_bytes_left = min(len, min(256, 256 - offset));
      bytes_read += block_bytes_left;
      current_addr += block_bytes_left;
      len -= block_bytes_left;
      translate_address(current_addr, &disk_num, &block_num, &offset);
      if (seek(disk_num, block_num) != 1) {
      return -1;
      }
    }
    // Reading and inserting into cache
    else {
      uint8_t my_buf[256];
      uint32_t op = encode_op(JBOD_READ_BLOCK, 0, 0);
      jbod_client_operation(op, my_buf);
      // Calculates how many bits left in the block if there is offset
      int block_bytes_left = min(len, min(256, 256 - offset));
      // Copies memory with appropriate offset for both buffers
      memcpy(buf + bytes_read, my_buf + offset, block_bytes_left);
      bytes_read += block_bytes_left;
      len -= block_bytes_left;
      current_addr += block_bytes_left;

      // Inserts buffer into cache
      if (cache_enabled()) {
        cache_insert(disk_num, block_num, buf);
      }

      translate_address(current_addr, &disk_num, &block_num, &offset);
      // Seeks to new block even if it is on a different disk
      if (seek(disk_num, block_num) != 1) {
        return -1;
      }
    }
  }   
  return bytes_read;
}

int mdadm_write(uint32_t addr, uint32_t len, const uint8_t *buf) {
  int space = len + addr;
  // Effectively tracks both read and wriiten bytes
  int bytes_written = 0;
  if (len > 1024 || space > 1048576 || is_mounted == 0 || (buf == NULL && len > 0)) {
    return -1;
  }
  uint32_t current_addr = addr;
  int disk_num, block_num, offset;
  translate_address(addr, &disk_num, &block_num, &offset);
  if (seek(disk_num, block_num) != 1) {
    return -1;
  }
  while (space > current_addr) {
    uint8_t my_buf[256];
    if (cache_lookup(disk_num, block_num, my_buf) == 1) {
      int block_bytes_left = min(len, min(256, 256 - offset));
      bytes_written += block_bytes_left;
      current_addr += block_bytes_left;
      len -= block_bytes_left;
      if (seek(disk_num, block_num) != 1) {
      return -1;
      }
    }
    // Manually reading and inserting
    else {
      uint32_t op_read = encode_op(JBOD_READ_BLOCK, 0, 0);
      jbod_client_operation(op_read, my_buf);
      int block_bytes_left = min(len, min(256, 256 - offset));
      // Copies appropriate memory from constant buffer to the temporary buffer
      memcpy(my_buf + offset, buf + bytes_written, block_bytes_left);
      bytes_written += block_bytes_left;
      len -= block_bytes_left;
      current_addr += block_bytes_left;

      // Inserts buffer into cache
      if (cache_enabled()){
        cache_insert(disk_num, block_num, buf);
      }

      // Does not translate address in order to seek back to the original block and disk num for writing
      if (seek(disk_num, block_num) != 1) {
        return -1;
      }
    }
    uint32_t op_write = encode_op(JBOD_WRITE_BLOCK, 0, 0);
    // Writes what is stored in the temp buffer
    jbod_client_operation(op_write, my_buf);
    // Seeks to new block or disk
    translate_address(current_addr, &disk_num, &block_num, &offset);
    if (seek(disk_num, block_num) != 1) {
      return -1;
    }
  }
  return bytes_written;
}
