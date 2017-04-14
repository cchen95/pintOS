#ifndef FILESYS_CACHE_H
#define FILESYS_CACHE_H

#include "filesys/off_t.h"

void acquire_cache_block (block_sector_t sector);
void release_cache_block (block_sector_t sector);
off_t read_cache_block (block_sector_t sector);
off_t write_cache_block (block_sector_t sector);

#endif /* filesys/cache.h */
