#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "cache.h"
#include <assert.h>

static cache_entry_t *cache = NULL;
static int cache_size = 0;
static int clock = 0;
static int num_queries = 0;
static int num_hits = 0;
// Tracks if cache_update was successful
static int updated = 0;

int cache_create(int num_entries) {
  // Check criteria
  if (cache != NULL || num_entries < 2 || num_entries > 4096) {
    return -1;
  }
  // Allocate space for all potential entries
  cache = calloc(num_entries, sizeof(cache_entry_t));
  cache_size = num_entries;
  return 1;
}

int cache_destroy(void) {
  if (cache == NULL) {
    return -1;
  }
  // Free pointer and set to NULL
  free(cache);
  cache = NULL;
  cache_size = 0;
  clock = 0;
  return 1;
}

int cache_lookup(int disk_num, int block_num, uint8_t *buf) {
  if (cache == NULL || cache_size == 0 || clock == 0 || buf == NULL) {
    return -1;
  }
  num_queries++;
  clock++;
  cache_entry_t* entry = cache;
  // Indexes the cache and checks if IDs match
  while (entry < cache + cache_size) {
    if (entry->disk_num == disk_num && entry->block_num == block_num) {
      num_hits++;
      // Updates access time
      entry->access_time = clock;
      // Copies data to buffer
      memcpy(buf, entry->block, JBOD_BLOCK_SIZE);
      return 1;
    }
    entry++;
  }
  return -1;
}

void cache_update(int disk_num, int block_num, const uint8_t *buf) {
  cache_entry_t* entry = cache;
  while (entry < cache + cache_size) {
    if (entry->disk_num == disk_num && entry->block_num == block_num) {
      // Copies data to cache block
      memcpy(entry->block, buf, JBOD_BLOCK_SIZE);
      entry->access_time = clock;
    }
    entry++;
  }
}

int cache_insert(int disk_num, int block_num, const uint8_t *buf) {
  if (cache == NULL || buf == NULL || disk_num < 0 || disk_num > 16 || block_num < 0 || block_num > 256) {
    return -1;
  }
  clock++;
  cache_update(disk_num, block_num, buf);
  if (updated == 1) {
    updated = 0;
    return -1;
  }
  
  // Initializes new entry
  cache_entry_t new_entry;
  new_entry.disk_num = disk_num;
  new_entry.block_num = block_num;
  new_entry.access_time = clock;
  new_entry.valid = true;
  memcpy(new_entry.block, buf, JBOD_BLOCK_SIZE);
  
  //Traverses list to find open position in cache
  cache_entry_t* available_spot = cache;
  while (available_spot < cache + cache_size) {
    // If spot is available, keeps pointer on that entry
    if (!(available_spot->valid)) {
      break;
    }
    // If entry is already in cache it will be updated
    if (available_spot->disk_num == disk_num && available_spot->block_num == block_num) {
      cache_update(disk_num, block_num, buf);
      return -1;
  }
    available_spot++;
  }

  // If eviction policy must be used
  if (available_spot >= cache + cache_size) {
    // Traverse cache to find the lowest access time
    cache_entry_t* entry = cache + 1;
    cache_entry_t* lowest_time = cache;
    while (entry < cache + cache_size) {
      if (entry->access_time < lowest_time->access_time) {
        lowest_time = entry;
      }
      entry++;
    }
    *lowest_time = new_entry;
    return 1;
  }

  // Appends new_entry to cache
  *available_spot = new_entry;
  return 1;
}

bool cache_enabled(void){
  if (cache != NULL) {
    return true;
  }
  return false;
}


void cache_print_hit_rate(void) {
  fprintf(stderr, "Hit rate: %5.1f%%\n", 100 * (float) num_hits / num_queries);
}