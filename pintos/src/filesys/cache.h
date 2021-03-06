#ifndef FILESYS_CACHE_H
#define FILESYS_CACHE_H

#include "filesys/off_t.h"
#include "devices/block.h"

void cache_init (void);
struct cache_block *init_cache_block (block_sector_t sector);
struct cache_block *find_cache_block (block_sector_t sector);
void evict_block (struct cache_block *cb);
void update_lru (struct cache_block *cb);
uint8_t* read_cache_block (block_sector_t sector, void *buffer, off_t offset, off_t size);
uint8_t* write_cache_block (block_sector_t sector, void *buffer, off_t offset, off_t size);

void free_cache (void);
struct cache_block *get_data (block_sector_t sector);

//testing
void cache_stats(int *hits, int *misses);

#endif /* filesys/cache.h */
