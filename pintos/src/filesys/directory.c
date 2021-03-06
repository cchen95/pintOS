#include "filesys/directory.h"
#include <stdio.h>
#include <string.h>
#include <list.h>
#include "filesys/filesys.h"
#include "filesys/inode.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"

/* A directory. */
struct dir
  {
    struct inode *inode;                /* Backing store. */
    off_t pos;                          /* Current position. */
  };

/* A single directory entry. */
struct dir_entry
  {
    block_sector_t inode_sector;        /* Sector number of header. */
    char name[NAME_MAX + 1];            /* Null terminated file name. */
    bool in_use;                        /* In use or free? */
  };

/* Creates a directory with space for ENTRY_CNT entries in the
   given SECTOR.  Returns true if successful, false on failure. */
bool
dir_create (block_sector_t sector, size_t entry_cnt)
{
  bool success = inode_create (sector, entry_cnt * sizeof (struct dir_entry));
  if (!success || sector != ROOT_DIR_SECTOR)
    return success;

  /* Set root_dir is_dir to true */
  struct inode *root_inode = inode_open (ROOT_DIR_SECTOR);
  inode_set_dir (root_inode, true);
  inode_close (root_inode);
  return success;
}

/* Opens and returns the directory for the given INODE, of which
   it takes ownership.  Returns a null pointer on failure. */
struct dir *
dir_open (struct inode *inode)
{
  struct dir *dir = calloc (1, sizeof *dir);
  if (inode != NULL && dir != NULL)
    {
      dir->inode = inode;
      dir->pos = 0;
      return dir;
    }
  else
    {
      inode_close (inode);
      free (dir);
      return NULL;
    }
}

/* Opens the root directory and returns a directory for it.
   Return true if successful, false on failure. */
struct dir *
dir_open_root (void)
{
  return dir_open (inode_open (ROOT_DIR_SECTOR));
}

/* Opens and returns a new directory for the same inode as DIR.
   Returns a null pointer on failure. */
struct dir *
dir_reopen (struct dir *dir)
{
  return dir_open (inode_reopen (dir->inode));
}

/* Destroys DIR and frees associated resources. */
void
dir_close (struct dir *dir)
{
  if (dir != NULL)
    {
      inode_close (dir->inode);
      free (dir);
    }
}

/* Returns the inode encapsulated by DIR. */
struct inode *
dir_get_inode (struct dir *dir)
{
  return dir->inode;
}

/* Searches DIR for a file with the given NAME.
   If successful, returns true, sets *EP to the directory entry
   if EP is non-null, and sets *OFSP to the byte offset of the
   directory entry if OFSP is non-null.
   otherwise, returns false and ignores EP and OFSP. */
static bool
lookup (const struct dir *dir, const char *name,
        struct dir_entry *ep, off_t *ofsp)
{
  struct dir_entry e;
  size_t ofs;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  for (ofs = 0; inode_read_at (dir->inode, &e, sizeof e, ofs) == sizeof e;
       ofs += sizeof e)
    if (e.in_use && !strcmp (name, e.name))
      {
        if (ep != NULL)
          *ep = e;
        if (ofsp != NULL)
          *ofsp = ofs;
        return true;
      }
  return false;
}

/* Searches DIR for a file with the given NAME
   and returns true if one exists, false otherwise.
   On success, sets *INODE to an inode for the file, otherwise to
   a null pointer.  The caller must close *INODE. */
bool
dir_lookup (const struct dir *dir, const char *name,
            struct inode **inode)
{
  struct dir_entry e;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  struct inode *curr_inode = dir_get_inode (dir);
  if (!strcmp (name, "."))
    *inode = inode_reopen (curr_inode);
  else if (!strcmp (name, ".."))
    {
      if (inode_get_inumber (curr_inode) == ROOT_DIR_SECTOR)
        return false;
      *inode = inode_open (inode_get_parent (curr_inode));
    }
  else if (lookup (dir, name, &e, NULL))
    *inode = inode_open (e.inode_sector);
  else
    *inode = NULL;

  return *inode != NULL;
}

/* Adds a file named NAME to DIR, which must not already contain a
   file by that name.  The file's inode is in sector
   INODE_SECTOR.
   Returns true if successful, false on failure.
   Fails if NAME is invalid (i.e. too long) or a disk or memory
   error occurs. */
bool
dir_add (struct dir *dir, const char *name, block_sector_t inode_sector)
{
  struct dir_entry e;
  off_t ofs;
  bool success = false;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  /* Check NAME for validity. */
  if (*name == '\0' || strlen (name) > NAME_MAX)
    return false;

  /* Set OFS to offset of free slot.
     If there are no free slots, then it will be set to the
     current end-of-file.

     inode_read_at() will only return a short read at end of file.
     Otherwise, we'd need to verify that we didn't get a short
     read due to something intermittent such as low memory. */
  bool found = false;
  for (ofs = 0; inode_read_at (dir->inode, &e, sizeof e, ofs) == sizeof e;
       ofs += sizeof e)
       {
         if (e.in_use && !strcmp (name, e.name))
          found = true;
         if (!e.in_use)
          break;
       }

  if (found)
    goto done;

  /* Write slot. */
  e.in_use = true;
  strlcpy (e.name, name, sizeof e.name);
  e.inode_sector = inode_sector;
  success = inode_write_at (dir->inode, &e, sizeof e, ofs) == sizeof e;

 done:
  return success;
}

/* Removes any entry for NAME in DIR.
   Returns true if successful, false on failure,
   which occurs only if there is no file with the given NAME. */
bool
dir_remove (struct dir *dir, const char *name)
{
  struct dir_entry e;
  struct inode *inode = NULL;
  bool success = false;
  off_t ofs;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  /* Find directory entry. */
  if (!lookup (dir, name, &e, &ofs))
    goto done;

  /* Open inode. */
  inode = inode_open (e.inode_sector);
  if (inode == NULL)
    goto done;

  /* Erase directory entry. */
  e.in_use = false;
  if (inode_write_at (dir->inode, &e, sizeof e, ofs) != sizeof e)
    goto done;

  /* Remove inode. */
  inode_remove (inode);
  success = true;

 done:
  inode_close (inode);
  return success;
}

/* Reads the next directory entry in DIR and stores the name in
   NAME.  Returns true if successful, false if the directory
   contains no more entries. */
bool
dir_readdir (struct dir *dir, char name[NAME_MAX + 1])
{
  struct dir_entry e;
  while (inode_read_at (dir->inode, &e, sizeof e, dir->pos) == sizeof e)
    {
      dir->pos += sizeof e;
      if (e.in_use)
        {
          strlcpy (name, e.name, NAME_MAX + 1);
          return true;
        }
    }
  return false;
}

bool
dir_is_empty (struct dir *dir)
{
  struct dir_entry e;
  off_t pos = 0;

  while (inode_read_at (dir->inode, &e, sizeof e, pos) == sizeof e)
    {
      if (e.in_use)
        return false;
      pos += sizeof e;
    }
  return true;
}

size_t
num_dir_entries (void)
{
  return BLOCK_SECTOR_SIZE / sizeof (struct dir_entry);
}

/* Extracts a file name part from *SRCP into PART, and updates *SRCP so that the
next call will return the next file name part. Returns 1 if successful, 0 at
end of string, -1 for a too-long file name part. */
static int
get_next_part (char part[NAME_MAX + 1], const char **srcp)
{
  const char *src = *srcp;
  char *dst = part;

  /* Skip leading slashes. If it’s all slashes, we’re done. */
  while (*src == '/')
    src++;
  if (*src == '\0')
    return 0;

  /* Copy up to NAME_MAX character from SRC to DST. Add null terminator. */
  while (*src != '/' && *src != '\0')
    {
      if (dst < part + NAME_MAX)
        *dst++ = *src;
      else
        return -1;
      src++;
    }
  *dst = '\0';

  /* Advance source pointer. */
  *srcp = src;
  return 1;
}

/* Helper method for locating dir/file with a directory. Will create a struct dir
or NULL, to be closed by the caller after use*/
struct dir *
dir_find (struct dir *dir, const char *filepath, char filename[NAME_MAX + 1])
{
  /* Empty string */
  if (strcmp (filepath, "") == 0)
    return NULL;

  memset (filename, 0, NAME_MAX + 1);

  /* Need to close curr_dir after looking through */
  struct dir *curr_dir, *old_dir = NULL;
  if (filepath == NULL)
    return NULL;
  else if (filepath[0] == '/')
    curr_dir = dir_open_root ();
  else
    curr_dir = dir_reopen (dir);

  /* If case string is full of empty slashes */
  old_dir = dir_reopen (curr_dir);
  struct inode *inode = NULL;

  int n;
  while ((n = get_next_part (filename, &filepath)) == 1)
    {
      dir_close (old_dir);
      bool found_dir = dir_lookup (curr_dir, filename, &inode);

      /* If not found and no more parts return. filename is the file/dir to be created */
      if (!found_dir)
        {
          if (get_next_part (filename, &filepath) == 0)
            return curr_dir;
          else
            {
              dir_close (curr_dir);
              return NULL;
            }
        }

      /* If found entry, need to check if entry is file or directory
      If file, need to check that it is last component of filepath */
      if (!inode_is_dir (inode))
        {
          if (get_next_part (filename, &filepath) != 0)
            {
              dir_close (curr_dir);
              return NULL;
            }
          return curr_dir;
        }

      /* Close previous directory iterating through, open new one to look through*/
      old_dir = curr_dir;
      curr_dir = dir_open (inode);
    }

  dir_close (curr_dir);
  if (n == -1)
    {
      dir_close (old_dir);
      return NULL;
    }
  return old_dir;
}

bool
dir_add_dir (struct dir *dir, char name[NAME_MAX + 1])
{
  block_sector_t inode_sector = 0;
  bool success = (dir != NULL
                  && free_map_allocate (1, &inode_sector)
                  && dir_create (inode_sector, num_dir_entries ())
                  && dir_add (dir, name, inode_sector));

  if (!success)
    {
      if (inode_sector != 0)
        free_map_release (inode_sector, 1);
      return false;
    }

  /* Add parent sector to inode and set is_dir to true */
  struct inode *self_inode = inode_open (inode_sector);
  inode_set_dir (self_inode, true);
  block_sector_t parent_sector = inode_get_inumber (dir_get_inode (dir));
  inode_set_parent (self_inode, parent_sector);
  inode_close (self_inode);
  return success;
}
