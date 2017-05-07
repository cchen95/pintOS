#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/cache.h"
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"
#include "threads/synch.h"

#include <stdio.h>
/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44
#define NUM_DIRECT_PTRS 122
#define NUM_PTRS_IN_BLOCK 128

/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk
  {
    block_sector_t direct[NUM_DIRECT_PTRS];  /* Direct pointer to data. */
    block_sector_t indirect;                 /* Indirect pointer. */
    block_sector_t doubly_indirect;          /* Doubly indirect pointer. */
    off_t length;                            /* File size in bytes. */
    unsigned magic;                          /* Magic number. */
    int is_dir;
    block_sector_t parent;
  };

/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}

/* In-memory inode. */
struct inode
  {
    struct list_elem elem;              /* Element in inode list. */
    block_sector_t sector;              /* Sector number of disk location. */
    int open_cnt;                       /* Number of openers. */
    bool removed;                       /* True if deleted, false otherwise. */
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
    struct inode_disk data;             /* Inode content. */
    struct lock lock;                   /* Lock */
    bool dirty;
  };

/* Structure to store data on indirect pointers. */
struct indirect_block
  {
    block_sector_t data[NUM_PTRS_IN_BLOCK];
  };

/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t
byte_to_sector (const struct inode *inode, off_t pos)
{
  ASSERT (inode != NULL);
  block_sector_t block;
  if (pos < inode->data.length)
    {
      off_t offset = pos / BLOCK_SECTOR_SIZE;
      off_t max = NUM_DIRECT_PTRS;
      if (offset < max)
        return inode->data.direct[offset];
      max += NUM_PTRS_IN_BLOCK;
      if (offset < max)
      {
        /* Get block data from cache. */
        struct indirect_block *indirect;
        indirect = calloc (1, sizeof (struct indirect_block));
        read_cache_block (inode->data.indirect, indirect, 0, BLOCK_SECTOR_SIZE);

        block = indirect->data[offset - NUM_DIRECT_PTRS];
        free (indirect);
        return block;
      }
      max += NUM_PTRS_IN_BLOCK * NUM_PTRS_IN_BLOCK;
      if (offset < max)
      {
        /* Same as indirect except read from cache twice. */
        struct indirect_block *indirect;
        indirect = calloc (1, sizeof (struct indirect_block));
        read_cache_block (inode->data.doubly_indirect, indirect, 0, BLOCK_SECTOR_SIZE);

        off_t index1 = (offset - (NUM_PTRS_IN_BLOCK + NUM_DIRECT_PTRS)) / NUM_PTRS_IN_BLOCK;
        off_t index2 = (offset - (NUM_PTRS_IN_BLOCK + NUM_DIRECT_PTRS)) % NUM_PTRS_IN_BLOCK;
        read_cache_block (indirect->data[index1], indirect, 0, BLOCK_SECTOR_SIZE);

        block = indirect->data[index2];
        free (indirect);
        return block;
      }
    }
  return -1;
}

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;
struct lock inode_list_lock;

/* Initializes the inode module. */
void
inode_init (void)
{
  list_init (&open_inodes);
  lock_init (&inode_list_lock);
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (block_sector_t sector, off_t length)
{
  struct inode_disk *disk_inode = NULL;
  bool success = false;

  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);

  disk_inode = calloc (1, sizeof *disk_inode);
  if (disk_inode != NULL)
    {
      size_t sectors = bytes_to_sectors (length);
      disk_inode->length = length;
      disk_inode->magic = INODE_MAGIC;
      disk_inode->is_dir = false;
      if (inode_allocate (sectors, disk_inode))
        {
          write_cache_block (sector, disk_inode, 0, BLOCK_SECTOR_SIZE);
          success = true;
        }
      free (disk_inode);
    }
  return success;
}

bool
inode_allocate (size_t cnt, struct inode_disk *disk_inode)
{
  if (cnt == 0)
    return true;
  static char zeros[BLOCK_SECTOR_SIZE];
  /* First check direct pointers. */
  size_t i;
  for (i = 0; i < NUM_DIRECT_PTRS; i++)
    {
      if (disk_inode->direct[i] == 0)
        {
          if (free_map_allocate (1, &disk_inode->direct[i]))
            {
              write_cache_block (disk_inode->direct[i], zeros, 0, BLOCK_SECTOR_SIZE);
              cnt -= 1;
            }
          else
            return false;
        }
      if (cnt == 0)
        return true;
    }

  /* Indirect pointers */
  struct indirect_block indirect;
  if (disk_inode->indirect == 0)
    {
      free_map_allocate (1, &disk_inode->indirect);
      write_cache_block (disk_inode->indirect, zeros, 0, BLOCK_SECTOR_SIZE);
    }
  read_cache_block (disk_inode->indirect, &indirect, 0, BLOCK_SECTOR_SIZE);
  for (i = 0; i < NUM_PTRS_IN_BLOCK; i++)
    {
      if (indirect.data[i] == 0)
        {
          if (free_map_allocate (1, &indirect.data[i]))
            {
              write_cache_block (indirect.data[i], zeros, 0, BLOCK_SECTOR_SIZE);
              cnt -= 1;
            }
          else
            return false;
        }
      if (cnt == 0)
        {
          write_cache_block (disk_inode->indirect, &indirect, 0, BLOCK_SECTOR_SIZE);
          return true;
        }
    }
  write_cache_block (disk_inode->indirect, &indirect, 0, BLOCK_SECTOR_SIZE);


  struct indirect_block *doubly_indirect;
  doubly_indirect = calloc (1, sizeof (struct indirect_block));
  if (disk_inode->doubly_indirect == 0)
    {
      free_map_allocate (1, &disk_inode->doubly_indirect);
      write_cache_block (disk_inode->doubly_indirect, zeros, 0, BLOCK_SECTOR_SIZE);
    }
  read_cache_block (disk_inode->doubly_indirect, doubly_indirect, 0, BLOCK_SECTOR_SIZE);
  size_t j;
  for (i = 0; i < NUM_PTRS_IN_BLOCK; i++)
    {
      if (doubly_indirect->data[i] == 0)
        {
          free_map_allocate (1, &doubly_indirect->data[i]);
          write_cache_block (doubly_indirect->data[i], zeros, 0, BLOCK_SECTOR_SIZE);
        }
      read_cache_block (doubly_indirect->data[i], &indirect, 0, BLOCK_SECTOR_SIZE);
      for (j = 0; j < NUM_PTRS_IN_BLOCK; j++)
        {
          if (indirect.data[j] == 0)
            {
              if (free_map_allocate (1, &indirect.data[j]))
                {
                  write_cache_block (indirect.data[j], zeros, 0, BLOCK_SECTOR_SIZE);
                  cnt -= 1;
                }
              else
                return false;
            }
          if (cnt == 0)
            {
              write_cache_block (doubly_indirect->data[i], &indirect, 0, BLOCK_SECTOR_SIZE);
              write_cache_block (disk_inode->doubly_indirect, doubly_indirect, 0, BLOCK_SECTOR_SIZE);
              free (doubly_indirect);
              return true;
            }
        }
        write_cache_block (doubly_indirect->data[i], &indirect, 0, BLOCK_SECTOR_SIZE);
    }
  free (doubly_indirect);
  return false;
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (block_sector_t sector)
{
  struct list_elem *e;
  struct inode *inode;

  /* Need to check cache first? */

  /* Check whether this inode is already open. */
  lock_acquire (&inode_list_lock);
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e))
    {
      inode = list_entry (e, struct inode, elem);
      if (inode->sector == sector)
        {
          inode_reopen (inode);
          lock_release (&inode_list_lock);
          return inode;
        }
    }

  lock_release (&inode_list_lock);

  /* Allocate memory. */
  inode = malloc (sizeof *inode);
  if (inode == NULL)
    return NULL;

  /* Initialize. */
  lock_acquire (&inode_list_lock);
  list_push_front (&open_inodes, &inode->elem);
  lock_release (&inode_list_lock);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;
  inode->dirty = false;
  lock_init (&inode->lock);

  /* Read inode_disk data */
  read_cache_block (inode->sector, &inode->data, 0, BLOCK_SECTOR_SIZE);
  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  if (inode != NULL)
    lock_acquire (&inode->lock);
    inode->open_cnt++;
    lock_release (&inode->lock);
  return inode;
}

/* Returns INODE's inode number. */
block_sector_t
inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
}

int
inode_get_open_cnt (const struct inode *inode)
{
  return inode->open_cnt;
}

/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode)
{
  /* Ignore null pointer. */
  if (inode == NULL)
    return;

  /* Release resources if this was the last opener. */
  lock_acquire (&inode->lock);
  if (--inode->open_cnt == 0)
    {
      /* Remove from inode list and release lock. */
      lock_acquire (&inode_list_lock);
      list_remove (&inode->elem);
      lock_release (&inode_list_lock);

      /* If dirty, write block metadata to disk*/
      if (inode->dirty)
        {
          write_cache_block (inode->sector, &inode->data, 0, BLOCK_SECTOR_SIZE);
          inode_write_to_disk (inode);
        }

      /* Deallocate blocks if removed. */
      if (inode->removed)
        {
          free_map_release (inode->sector, 1);
          inode_release (&inode->data);
        }
      lock_release (&inode->lock);
      free (inode);
    }
  else
    lock_release (&inode->lock);
}

void
inode_write_to_disk (struct inode *inode)
{
  write_cache_block (inode->sector, &inode->data, 0, BLOCK_SECTOR_SIZE);

  struct inode_disk *disk = &inode->data;
  struct indirect_block indirect;
  if (disk->indirect != 0)
    {
      read_cache_block (disk->indirect, &indirect, 0, BLOCK_SECTOR_SIZE);
      block_write (fs_device, disk->indirect, &indirect);
    }

  if (disk->doubly_indirect != 0)
    {
      struct indirect_block *doubly_indirect;
      doubly_indirect = calloc (1, sizeof (struct indirect_block));
      read_cache_block (disk->doubly_indirect, &doubly_indirect, 0, BLOCK_SECTOR_SIZE);
      size_t i;
      for (i = 0; i < NUM_PTRS_IN_BLOCK; i++)
        {
          read_cache_block (doubly_indirect->data[i], &indirect, 0, BLOCK_SECTOR_SIZE);
          block_write (fs_device, doubly_indirect->data[i], &indirect);
        }
      free (doubly_indirect);
    }
}

void
inode_release (struct inode_disk *disk)
{
  size_t i;
  for (i = 0; i < NUM_DIRECT_PTRS; i++)
    if (disk->direct[i] != 0)
      free_map_release (disk->direct[i], 1);

  struct indirect_block indirect;
  if (disk->indirect != 0)
    {
      for (i = 0; i < NUM_PTRS_IN_BLOCK; i++)
        {
          read_cache_block (disk->indirect, &indirect, 0, BLOCK_SECTOR_SIZE);
          if (indirect.data[i] != 0)
            free_map_release (indirect.data[i], 1);
        }
    }

  if (disk->doubly_indirect != 0)
    {
      struct indirect_block *doubly_indirect;
      doubly_indirect = calloc (1, sizeof (struct indirect_block));
      read_cache_block (disk->doubly_indirect, doubly_indirect, 0, BLOCK_SECTOR_SIZE);
      size_t j;
      for (i = 0; i < NUM_PTRS_IN_BLOCK; i++)
        {
          read_cache_block (doubly_indirect->data[i], &indirect, 0, BLOCK_SECTOR_SIZE);
          for (j = 0; j < NUM_PTRS_IN_BLOCK; j++)
            {
              if (indirect.data[j] != 0)
                free_map_release (indirect.data[j], 1);
            }
        }
      free (doubly_indirect);
    }
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode)
{
  ASSERT (inode != NULL);
  inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset)
{
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;
  uint8_t *bounce = NULL;

  int test = 0;
  if (offset == 61440)
    {
      test += 1;
      test += 1;
    }

  while (size > 0)
    {
      /* Disk sector to read, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      if (sector_idx == -1u)
        return bytes_read;
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      bounce = read_cache_block (sector_idx, buffer + bytes_read, sector_ofs, chunk_size);
      if (bounce == NULL)
        break;
      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
    }

  return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset)
{
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;
  uint8_t *bounce = NULL;

  if (inode->deny_write_cnt)
    return 0;

  /* Extend file. */
  if (offset + size > inode->data.length)
    {
      bool success = inode_allocate (bytes_to_sectors (offset + size - inode->data.length), &inode->data);
      if (!success)
        return 0;

      inode->data.length = offset + size;
      write_cache_block (inode->sector, &inode->data, 0, BLOCK_SECTOR_SIZE);
    }

  while (size > 0)
    {
      /* Sector to write, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      if (sector_idx == -1u)
        return bytes_written;
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      /* We need a bounce buffer. */
      bounce = write_cache_block (sector_idx, buffer + bytes_written, sector_ofs, chunk_size);
      if (bounce == NULL)
        break;

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
    }

  return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode)
{
  inode->deny_write_cnt++;
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode)
{
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
  return inode->data.length;
}

bool
inode_is_dir (struct inode *inode)
{
  return inode->data.is_dir;
}

void
inode_set_dir (struct inode *inode, bool is_dir)
{
  inode->dirty = true;
  inode->data.is_dir = is_dir;
}

block_sector_t
inode_get_parent (struct inode *inode)
{
  return inode->data.parent;
}

void
inode_set_parent (struct inode *inode, block_sector_t parent_sector)
{
  inode->dirty = true;
  inode->data.parent = parent_sector;
}
