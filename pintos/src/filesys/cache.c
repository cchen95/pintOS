#include <debug.h>
#include <list.h>
#include <string.h>
#include "filesys/cache.h"
#include "filesys/inode.h"
#include "threads/malloc.h"
#include "filesys/filesys.h"
#include "threads/synch.h"

#include <stdio.h>
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

/* Initializes the cache module. */
void
cache_init (void)
{
  list_init (&cache_list);
  lock_init (&cache_lock);
}

/* Initializes struct cache_block. */
struct cache_block *
init_cache_block (block_sector_t sector)
{
  struct cache_block *cb = malloc (sizeof (struct cache_block));
  if (cb == NULL)
    {
      free (cb);
      return NULL;
    }
  cb->dirty = 0;
  cb->sector = sector;
  lock_init (&cb->block_lock);
  return cb;
}

void
free_cache (void)
{
  lock_acquire (&cache_lock);
  while (!list_empty (&cache_list))
    {
      evict_block ();
    }
  lock_release (&cache_lock);
}

/* Returns the cache_block that has sector number sector if it is in cache_list.
   Else, returns NULL */
struct cache_block *
find_cache_block (block_sector_t sector)
{
  struct list_elem *e;
  for (e = list_begin (&cache_list); e != list_end (&cache_list);
       e = list_next (e))
    {
      struct cache_block *cb = list_entry(e, struct cache_block, elem);
      if (cb->sector == sector)
        return cb;
    }
  return NULL;
}

/* Find and return block to evict. */
struct cache_block *
evict_block (void)
{
  if (list_empty (&cache_list))
    return NULL;
  struct list_elem *e = list_pop_back (&cache_list);
  struct cache_block *cb = list_entry(e, struct cache_block, elem);
  lock_acquire (&cb->block_lock);
  if (cb->dirty)
    {
      block_write(fs_device, cb->sector, cb->data);
    }
  lock_release (&cb->block_lock);
  free(cb);
  return NULL;
}


/* Updates cache_list using LRU policy.
   Removes elem from list first if in_list == 1.
   Then pushes elem to front of the list. */
void
update_lru (struct cache_block *cb)
{
  lock_acquire (&cache_lock);
  struct cache_block *found = find_cache_block (cb->sector);
  if (found != NULL)
    list_remove (&found->elem);
  list_push_front (&cache_list, &cb->elem);
  lock_release (&cache_lock);
}


struct cache_block *
get_data (block_sector_t sector)
{
  lock_acquire (&cache_lock);
  struct cache_block *cb = find_cache_block(sector);
  int size = list_size (&cache_list);
  lock_release (&cache_lock);
  if (!cb)
    {
      if (size >= 64)
        {
          lock_acquire (&cache_lock);
          evict_block ();
          lock_release (&cache_lock);
        }
      cb = init_cache_block (sector);
      if (cb == NULL)
          return NULL;
      /* Load in new block from disk into cb->data. */
      lock_acquire (&cb->block_lock);
      block_read (fs_device, sector, cb->data);
      cb->dirty = 0;
      cb->sector = sector;
      lock_release (&cb->block_lock);
    }
  return cb;
}

uint8_t *
read_cache_block (block_sector_t sector)
{
  struct cache_block *cb = get_data (sector);
  if (cb == NULL)
    return NULL;

  update_lru (cb);
  return cb->data;
}

uint8_t *
write_cache_block (block_sector_t sector)
{
  struct cache_block *cb = get_data (sector);
  if (cb == NULL)
    return NULL;

  /* Write buffer into cb->data */
  lock_acquire (&cb->block_lock);

  cb->sector = sector;
  cb->dirty = 1;

  lock_release (&cb->block_lock);

  update_lru (cb);
  return cb->data;
}
