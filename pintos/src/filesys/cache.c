#include "filesys/cache.h"
#include <debug.h>
#include <list.h>
#include "filesys/inode.h"
#include "threads/malloc.h"


struct cache_block
  {
    int dirty;                                    /* Dirty bit */
    int users;                                    /* Number of users accessing the block */
    struct lock block_lock;                       /* Lock for block */
    struct list_elem block_elem;                  /* Element in blocks_list */
    block_sector_t data[64 * BLOCK_SECTOR_SIZE];  /* Store data here */
  }

/* List of blocks in the cache. */
struct list blocks_list;

/* Lock for the cache. */
struct lock cache_lock;

void
acquire_cache_block (block_sector_t sector)
{

}

void
release_cache_block (block_sector_t sector)
{

}

off_t
read_cache_block (block_sector_t sector)
{

}

off_t
write_cache_block (block_sector_t sector)
{

}
