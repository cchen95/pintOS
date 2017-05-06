#include <debug.h>
#include <list.h>
#include <string.h>
#include "filesys/cache.h"
#include "filesys/inode.h"
#include "threads/malloc.h"
#include "filesys/filesys.h"
#include "threads/synch.h"

#include <stdio.h>
#define CACHE_SIZE 64

struct cache_block
  {
    int dirty;                        /* Dirty bit */
    struct lock block_lock;           /* Lock for block */
    struct list_elem elem;            /* Element in cache_list */
    block_sector_t sector;            /* Sector number for this block */
    uint8_t data[BLOCK_SECTOR_SIZE];  /* Store data here */
  };

/* List of blocks in the cache. */
static struct list cache_list;

/* Lock for the cache. */
struct lock cache_lock;

/* Testing buffer cache's effectiveness */
// might need lock for this
size_t cache_hit;
size_t cache_miss;

/* Initializes the cache module. */
void
cache_init (void)
{
  list_init (&cache_list);
  lock_init (&cache_lock);

  int i;
  struct cache_block *cb;
  lock_acquire (&cache_lock);
  for (i = 0; i < CACHE_SIZE; i++)
    {
      cb = malloc (sizeof (struct cache_block));
      if (cb == NULL)
        {
          free (cb);
          return;
        }
      cb->dirty = 0;
      cb->sector = 0;
      lock_init (&cb->block_lock);
      list_push_front (&cache_list, &cb->elem);
    }
  lock_release (&cache_lock);
  cache_hit = 0;
  cache_miss = 0;
}

void
free_cache (void)
{
  struct list_elem *e;
  struct cache_block *cb;
  lock_acquire (&cache_lock);
  while (!list_empty (&cache_list))
    {
      e = list_pop_back (&cache_list);
      cb = list_entry (e, struct cache_block, elem);
      evict_block (cb);
      free (cb);
    }
  lock_release (&cache_lock);
  cache_hit = 0;
  cache_miss = 0;
}

/* Returns the cache_block that has sector number sector if it is in cache_list.
   Else, returns NULL */
struct cache_block *
find_cache_block (block_sector_t sector)
{
  struct list_elem *e;
  lock_acquire (&cache_lock);
  for (e = list_begin (&cache_list); e != list_end (&cache_list);
       e = list_next (e))
    {
      struct cache_block *cb = list_entry (e, struct cache_block, elem);
      if (cb->sector == sector)
        {
          lock_release (&cache_lock);
          return cb;
        }
    }
  lock_release (&cache_lock);
  return NULL;
}

/* Evict last block in cache_list. */
void
evict_block (struct cache_block *cb)
{
  lock_acquire (&cb->block_lock);
  if (cb->dirty)
    {
      block_write (fs_device, cb->sector, cb->data);
    }
  lock_release (&cb->block_lock);
}


/* Updates cache_list using LRU policy.
*/
void
update_lru (struct cache_block *cb)
{
  lock_acquire (&cache_lock);
  list_remove (&cb->elem);
  list_push_front (&cache_list, &cb->elem);
  lock_release (&cache_lock);
}


struct cache_block *
get_data (block_sector_t sector)
{
  struct cache_block *cb = find_cache_block (sector);
  if (cb)
    {
      update_lru (cb);
      cache_hit++;
    }
  if (!cb)
    {
      lock_acquire (&cache_lock);
      struct list_elem *e = list_pop_back (&cache_list);
      cb = list_entry (e, struct cache_block, elem);
      lock_release (&cache_lock);

      /* Check if block is dirty and write to disk. */
      evict_block (cb);

      /* Load in new block from disk into cb->data. */
      lock_acquire (&cb->block_lock);
      block_read (fs_device, sector, cb->data);
      cb->dirty = 0;
      cb->sector = sector;
      lock_release (&cb->block_lock);

      /* Push block back to list */
      lock_acquire (&cache_lock);
      list_push_front (&cache_list, &cb->elem);
      lock_release (&cache_lock);
      cache_miss++;
    }
  return cb;
}

uint8_t *
read_cache_block (block_sector_t sector, void *buffer, off_t offset, off_t size)
{
  uint8_t *bounce = NULL;
  struct cache_block *cb = get_data (sector);
  if (cb == NULL)
    return NULL;
  bounce = cb->data;
  memcpy (buffer, bounce + offset, size);
  return bounce;
}

uint8_t *
write_cache_block (block_sector_t sector, void *buffer, off_t offset, off_t size)
{
  uint8_t *bounce = NULL;
  struct cache_block *cb = get_data (sector);
  if (cb == NULL)
    return NULL;

  /* Write buffer into cb->data */
  lock_acquire (&cb->block_lock);

  cb->sector = sector;
  cb->dirty = 1;

  lock_release (&cb->block_lock);
  bounce = cb->data;
  memcpy (bounce + offset, buffer, size);
  return bounce;
}
